#include "ksm_webview_host.h"

#include "ksm_webview_security.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

namespace {

using Json = nlohmann::json;

constexpr long long kDefaultTimeoutMs = 10000;

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

}

KSMWebViewHost::KSMWebViewHost(
    SendMessage send_message,
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services,
    Dispatch dispatch
)
    : send_message_(std::move(send_message)),
      dispatch_(std::move(dispatch)),
      router_(std::move(file_system), std::move(host_services)) {
    for (std::size_t index = 0; index < kMaxConcurrentCommands; ++index) {
        workers_.emplace_back([this] { WorkerLoop(); });
    }
    timer_thread_ = std::thread([this] { TimerLoop(); });
}

KSMWebViewHost::~KSMWebViewHost() {
    {
        std::lock_guard lock(queue_mutex_);
        for (const auto& [id, state] : active_requests_) {
            state->completed.store(true);
            router_.CancelRequest(id);
        }
        stopping_ = true;
    }
    queue_ready_.notify_all();
    {
        std::lock_guard lock(timer_mutex_);
        timer_stopping_ = true;
    }
    timer_ready_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    if (timer_thread_.joinable()) {
        timer_thread_.join();
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
            if (
                (!timeout.is_number_integer() && !timeout.is_number_unsigned()) ||
                timeout.get<long long>() <= 0
            ) {
                SendError(
                    RequestId(id),
                    "INVALID_ARGS",
                    "timeoutMs must be a positive integer.",
                    [this](const std::string& response) {
                        PostResponse(response);
                    }
                );
                return;
            }
            timeout_ms = timeout.get<long long>();
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

    RegisterRequest(state, timeout_ms);
    {
        std::lock_guard lock(queue_mutex_);
        tasks_.push_back([this, message, state] {
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
        });
    }
    queue_ready_.notify_one();
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

void KSMWebViewHost::RegisterRequest(
    const std::shared_ptr<RequestState>& state,
    long long timeout_ms
) {
    if (!state->id.empty()) {
        std::lock_guard lock(queue_mutex_);
        active_requests_[state->id] = state;
    }
    {
        std::lock_guard lock(timer_mutex_);
        deadlines_.push_back(Deadline{
            std::chrono::steady_clock::now() +
                std::chrono::milliseconds(timeout_ms),
            state
        });
    }
    timer_ready_.notify_one();
}

void KSMWebViewHost::UnregisterRequest(
    const std::shared_ptr<RequestState>& state
) {
    if (state->id.empty()) {
        return;
    }
    std::lock_guard lock(queue_mutex_);
    const auto iterator = active_requests_.find(state->id);
    if (iterator != active_requests_.end() && iterator->second == state) {
        active_requests_.erase(iterator);
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

void KSMWebViewHost::WorkerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(queue_mutex_);
            queue_ready_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });
            if (stopping_) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
    }
}

void KSMWebViewHost::TimerLoop() {
    while (true) {
        std::shared_ptr<RequestState> expired;
        {
            std::unique_lock lock(timer_mutex_);
            timer_ready_.wait(lock, [this] {
                return timer_stopping_ || !deadlines_.empty();
            });
            if (timer_stopping_) {
                return;
            }
            const auto next = std::min_element(
                deadlines_.begin(),
                deadlines_.end(),
                [](const Deadline& left, const Deadline& right) {
                    return left.at < right.at;
                }
            );
            if (next->at > std::chrono::steady_clock::now()) {
                timer_ready_.wait_until(lock, next->at);
                continue;
            }
            expired = next->state;
            deadlines_.erase(next);
        }
        if (!expired->completed.load()) {
            router_.CancelRequest(expired->id);
            RespondOnce(
                expired,
                Json{
                    {"type", "response"},
                    {"id", RequestId(expired->id)},
                    {"ok", false},
                    {"error", {
                        {"code", "TIMEOUT"},
                        {"message", "The request timed out."}
                    }}
                }.dump()
            );
        }
    }
}

void KSMWebViewHost::NotifyWebViewReloaded() {
    router_.EmitSystemEvent(
        "webview.reloaded",
        "{}",
        [this](const std::string& response) {
            PostResponse(response);
        }
    );
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

bool KSMWebViewHost::IsAllowedOrigin(const std::string& origin) const {
    return KSMWebViewSecurity::IsAllowedOrigin(origin);
}

bool KSMWebViewHost::IsAllowedNavigation(const std::string& url) const {
    return KSMWebViewSecurity::IsAllowedNavigation(url);
}
