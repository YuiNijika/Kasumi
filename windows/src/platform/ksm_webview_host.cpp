#include "ksm_webview_host.h"

#include "ksm_webview_security.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using Json = nlohmann::json;

constexpr long long kDefaultTimeoutMs = 10000;
constexpr long long kMaxTimeoutMs = 600000;

void SendError(
    const Json& id,
    const std::string& code,
    const std::string& message,
    const KSMWebViewHost::SendMessage& send_message
) {
    send_message(Json{
        {"type", "response"},
        {"id", id},
        {"ok", false},
        {"error", {{"code", code}, {"message", message}}}
    }.dump());
}

Json RequestId(const std::string& id) {
    return id.empty() ? Json(nullptr) : Json(id);
}

class WorkerPool {
public:
    static WorkerPool& Instance() {
        static WorkerPool pool;
        return pool;
    }

    bool Submit(std::function<void()> task) {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return false;
            }
            tasks_.push_back(std::move(task));
        }
        ready_.notify_one();
        return true;
    }

private:
    WorkerPool() {
        const std::size_t hardware = std::thread::hardware_concurrency();
        const std::size_t count = std::clamp<std::size_t>(
            hardware == 0 ? 2 : hardware,
            2,
            16
        );
        for (std::size_t index = 0; index < count; ++index) {
            workers_.emplace_back([this] { Run(); });
        }
    }

    ~WorkerPool() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void Run() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });
                if (stopping_) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try {
                task();
            } catch (...) {
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};

class TimerScheduler {
public:
    static TimerScheduler& Instance() {
        static TimerScheduler scheduler;
        return scheduler;
    }

    std::uint64_t Schedule(
        std::chrono::steady_clock::time_point at,
        std::function<void()> callback
    ) {
        std::lock_guard lock(mutex_);
        const std::uint64_t id = next_id_++;
        deadlines_.push(TimerTask{at, id, std::move(callback)});
        ready_.notify_one();
        return id;
    }

    void Cancel(std::uint64_t id) {
        if (id == 0) {
            return;
        }
        {
            std::lock_guard lock(mutex_);
            cancelled_.insert(id);
        }
        ready_.notify_one();
    }

private:
    struct TimerTask {
        std::chrono::steady_clock::time_point at;
        std::uint64_t id;
        std::function<void()> callback;
    };

    struct Later {
        bool operator()(const TimerTask& left, const TimerTask& right) const {
            return left.at > right.at;
        }
    };

    TimerScheduler()
        : thread_([this] { Run(); }) {
    }

    ~TimerScheduler() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void Run() {
        while (true) {
            std::function<void()> callback;
            {
                std::unique_lock lock(mutex_);
                while (true) {
                    if (stopping_) {
                        return;
                    }
                    while (
                        !deadlines_.empty() &&
                        cancelled_.erase(deadlines_.top().id) > 0
                    ) {
                        deadlines_.pop();
                    }
                    if (deadlines_.empty()) {
                        ready_.wait(lock);
                        continue;
                    }
                    const auto now = std::chrono::steady_clock::now();
                    if (deadlines_.top().at > now) {
                        ready_.wait_until(lock, deadlines_.top().at);
                        continue;
                    }
                    callback = std::move(deadlines_.top().callback);
                    deadlines_.pop();
                    break;
                }
            }
            try {
                callback();
            } catch (...) {
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable ready_;
    std::priority_queue<TimerTask, std::vector<TimerTask>, Later> deadlines_;
    std::unordered_set<std::uint64_t> cancelled_;
    std::uint64_t next_id_ = 1;
    bool stopping_ = false;
    std::thread thread_;
};

}

KSMWebViewHost::KSMWebViewHost(
    SendMessage send_message,
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services,
    Dispatch dispatch,
    std::vector<std::string> allowed_commands,
    bool allow_development_origin,
    std::vector<std::shared_ptr<KSMPlugin>> plugins,
    std::vector<std::string> allowed_permissions,
    std::string window_id
)
    : send_message_(std::move(send_message)),
      dispatch_(std::move(dispatch)),
      router_(
          std::move(file_system),
          std::move(host_services),
          allowed_commands,
          std::move(allowed_permissions),
          std::move(plugins),
          window_id
      ),
      window_id_(std::move(window_id)),
      allow_development_origin_(allow_development_origin) {
    router_.SetEventSender([this](const std::string& response) {
        PostResponse(response);
    });
}

KSMWebViewHost::~KSMWebViewHost() {
    std::vector<std::uint64_t> timers;
    {
        std::lock_guard lock(queue_mutex_);
        for (const auto& [id, state] : active_requests_) {
            state->completed.store(true);
            router_.CancelRequest(id);
            timers.push_back(state->timer_id);
        }
        active_requests_.clear();
    }
    for (const std::uint64_t timer : timers) {
        TimerScheduler::Instance().Cancel(timer);
    }
    {
        std::unique_lock lock(runtime_state_->mutex);
        runtime_state_->shutting_down = true;
        runtime_state_->idle.wait(lock, [this] {
            return runtime_state_->active_tasks == 0;
        });
    }
}

void KSMWebViewHost::OnMessageReceived(const std::string& message) {
    const Json request = Json::parse(message, nullptr, false);
    std::string id;
    long long timeout_ms = kDefaultTimeoutMs;
    if (request.is_object()) {
        const auto id_iterator = request.find("id");
        if (id_iterator != request.end() && id_iterator->is_string()) {
            id = id_iterator->get<std::string>();
        }
        const bool is_cancel =
            request.contains("type") &&
            request.at("type").is_string() &&
            request.at("type").get<std::string>() == "cancel";
        if (is_cancel && !id.empty()) {
            CancelRequest(id);
            return;
        }
        if (request.contains("timeoutMs")) {
            const auto& timeout = request.at("timeoutMs");
            bool valid_timeout = false;
            if (timeout.is_number_unsigned()) {
                const auto value = timeout.get<std::uint64_t>();
                valid_timeout =
                    value > 0 && value <= static_cast<std::uint64_t>(kMaxTimeoutMs);
                if (valid_timeout) {
                    timeout_ms = static_cast<long long>(value);
                }
            } else if (timeout.is_number_integer()) {
                const auto value = timeout.get<long long>();
                valid_timeout = value > 0 && value <= kMaxTimeoutMs;
                if (valid_timeout) {
                    timeout_ms = value;
                }
            }
            if (!valid_timeout) {
                SendError(
                    RequestId(id),
                    "INVALID_ARGS",
                    "timeoutMs must be an integer between 1 and 600000.",
                    [this](const std::string& response) {
                        PostResponse(response);
                    }
                );
                return;
            }
        }
    }

    auto state = std::make_shared<RequestState>();
    state->id = id;
    bool accepted = false;
    {
        std::lock_guard lock(queue_mutex_);
        if (in_flight_ < kMaxConcurrentCommands + kMaxQueuedCommands) {
            ++in_flight_;
            accepted = true;
        }
    }
    if (!accepted) {
        SendError(
            RequestId(id),
            "BUSY",
            "Too many concurrent commands.",
            [this](const std::string& response) {
                PostResponse(response);
            }
        );
        return;
    }

    if (!RegisterRequest(state, timeout_ms)) {
        {
            std::lock_guard lock(queue_mutex_);
            --in_flight_;
        }
        SendError(
            RequestId(id),
            "BUSY",
            "A request with this ID is already in flight.",
            [this](const std::string& response) {
                PostResponse(response);
            }
        );
        return;
    }
    if (!StartRuntimeTask(
            runtime_state_,
            [this, message, state] {
                if (!state->completed.load()) {
                    router_.HandleMessage(
                        message,
                        [this, state](const std::string& response) {
                            RespondOnce(state, response);
                        }
                    );
                }
                UnregisterRequest(state);
                {
                    std::lock_guard task_lock(queue_mutex_);
                    --in_flight_;
                }
            }
        )) {
        UnregisterRequest(state);
        {
            std::lock_guard lock(queue_mutex_);
            --in_flight_;
        }
        SendError(
            RequestId(id),
            "BUSY",
            "The command runtime is shutting down.",
            [this](const std::string& response) {
                PostResponse(response);
            }
        );
    }
}

void KSMWebViewHost::PostResponse(const std::string& message) {
    if (dispatch_) {
        const auto send = send_message_;
        dispatch_([send, message] { send(message); });
        return;
    }
    send_message_(message);
}

bool KSMWebViewHost::OnMessageReceivedFromOrigin(
    const std::string& message,
    const std::string& origin
) {
    if (!IsAllowedOrigin(origin)) {
        return false;
    }
    OnMessageReceived(message);
    return true;
}

void KSMWebViewHost::CancelRequest(const std::string& id) {
    std::shared_ptr<RequestState> state;
    {
        std::lock_guard lock(queue_mutex_);
        const auto iterator = active_requests_.find(id);
        if (iterator != active_requests_.end()) {
            state = iterator->second;
        }
    }
    if (state) {
        TimerScheduler::Instance().Cancel(state->timer_id);
        router_.CancelRequest(id);
        RespondOnce(
            state,
            Json{
                {"type", "response"},
                {"id", id},
                {"ok", false},
                {"error", {
                    {"code", "CANCELLED"},
                    {"message", "The request was cancelled."}
                }}
            }.dump()
        );
    }
}

bool KSMWebViewHost::RegisterRequest(
    const std::shared_ptr<RequestState>& state,
    long long timeout_ms
) {
    if (!state->id.empty()) {
        std::lock_guard lock(queue_mutex_);
        if (active_requests_.contains(state->id)) {
            return false;
        }
        active_requests_[state->id] = state;
    }
    state->timer_id = TimerScheduler::Instance().Schedule(
        std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms),
        [this, state, runtime = runtime_state_] {
            if (state->completed.load()) {
                return;
            }
            StartRuntimeTask(
                runtime,
                [this, state] {
                    if (!state->completed.load()) {
                        router_.CancelRequest(state->id);
                        RespondOnce(
                            state,
                            Json{
                                {"type", "response"},
                                {"id", RequestId(state->id)},
                                {"ok", false},
                                {"error", {
                                    {"code", "TIMEOUT"},
                                    {"message", "The request timed out."}
                                }}
                            }.dump()
                        );
                    }
                }
            );
        }
    );
    return true;
}

void KSMWebViewHost::ResetForNavigation() {
    std::vector<std::uint64_t> timers;
    {
        std::lock_guard lock(queue_mutex_);
        for (const auto& [id, state] : active_requests_) {
            state->completed.store(true);
            router_.CancelRequest(id);
            timers.push_back(state->timer_id);
        }
        active_requests_.clear();
    }
    for (const std::uint64_t timer : timers) {
        TimerScheduler::Instance().Cancel(timer);
    }
}

void KSMWebViewHost::UnregisterRequest(
    const std::shared_ptr<RequestState>& state
) {
    TimerScheduler::Instance().Cancel(state->timer_id);
    if (!state->id.empty()) {
        std::lock_guard lock(queue_mutex_);
        const auto iterator = active_requests_.find(state->id);
        if (iterator != active_requests_.end() && iterator->second == state) {
            active_requests_.erase(iterator);
        }
    }
}

void KSMWebViewHost::RespondOnce(
    const std::shared_ptr<RequestState>& state,
    const std::string& response
) {
    bool expected = false;
    if (!state->completed.compare_exchange_strong(expected, true)) {
        return;
    }
    PostResponse(response);
}

bool KSMWebViewHost::StartRuntimeTask(
    const std::shared_ptr<RuntimeState>& runtime,
    std::function<void()> task
) {
    {
        std::lock_guard lock(runtime->mutex);
        if (runtime->shutting_down) {
            return false;
        }
        ++runtime->active_tasks;
    }
    if (!WorkerPool::Instance().Submit(
            [runtime, task = std::move(task)] {
                try {
                    task();
                } catch (...) {
                }
                FinishRuntimeTask(runtime);
            }
        )) {
        FinishRuntimeTask(runtime);
        return false;
    }
    return true;
}

void KSMWebViewHost::FinishRuntimeTask(
    const std::shared_ptr<RuntimeState>& runtime
) {
    std::lock_guard lock(runtime->mutex);
    if (runtime->active_tasks > 0) {
        --runtime->active_tasks;
    }
    if (runtime->active_tasks == 0) {
        runtime->idle.notify_all();
    }
}

void KSMWebViewHost::NotifyWebViewNavigated(bool is_reload) {
    router_.EmitSystemEvent(
        "webview.navigated",
        "{}",
        [this](const std::string& response) {
            PostResponse(response);
        }
    );
    if (!is_reload) {
        return;
    }
    router_.EmitSystemEvent(
        "webview.reloaded",
        "{}",
        [this](const std::string& response) {
            PostResponse(response);
        }
    );
}

void KSMWebViewHost::NotifyWebViewReloaded() {
    NotifyWebViewNavigated(true);
}

void KSMWebViewHost::NotifyWindowResized(int width, int height) {
    router_.EmitEvent(
        "window.resized",
        nlohmann::json{{"width", width}, {"height", height}}.dump(),
        [this](const std::string& response) {
            PostResponse(response);
        }
    );
}

void KSMWebViewHost::EmitEvent(
    const std::string& event,
    const std::string& payload_json
) {
    router_.EmitEvent(
        event,
        payload_json,
        [this](const std::string& response) {
            PostResponse(response);
        }
    );
}

bool KSMWebViewHost::IsAllowedOrigin(const std::string& origin) const {
    return KSMWebViewSecurity::IsAllowedOrigin(
        origin,
        allow_development_origin_
    );
}

bool KSMWebViewHost::IsAllowedNavigation(const std::string& url) const {
    return KSMWebViewSecurity::IsAllowedNavigation(
        url,
        allow_development_origin_
    );
}
