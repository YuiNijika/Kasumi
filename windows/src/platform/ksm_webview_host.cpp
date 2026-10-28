#include "ksm_webview_host.h"

#include "ksm_webview_security.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace {

using Json = nlohmann::json;

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

}  // namespace

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
}

KSMWebViewHost::~KSMWebViewHost() {
    {
        std::lock_guard lock(queue_mutex_);
        stopping_ = true;
    }
    queue_ready_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void KSMWebViewHost::OnMessageReceived(const std::string& message) {
    Json request = Json::parse(message, nullptr, false);
    std::string id;
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
    }

    bool accepted = false;
    {
        std::lock_guard lock(queue_mutex_);
        if (in_flight_ < kMaxConcurrentCommands) {
            ++in_flight_;
            tasks_.push_back([this, message, id] {
                if (IsCancelled(id)) {
                    ClearCancelled(id);
                    SendError(
                        id.empty() ? Json(nullptr) : Json(id),
                        "CANCELLED",
                        "The request was cancelled.",
                        [this](const std::string& response) {
                            PostResponse(response);
                        }
                    );
                } else {
                    router_.HandleMessage(
                        message,
                        [this](const std::string& response) {
                            PostResponse(response);
                        }
                    );
                }
                {
                    std::lock_guard task_lock(queue_mutex_);
                    --in_flight_;
                }
            });
            accepted = true;
        }
    }
    if (accepted) {
        queue_ready_.notify_one();
        return;
    }

    SendError(
        id.empty() ? Json(nullptr) : Json(id),
        "BUSY",
        "Too many concurrent commands.",
        [this](const std::string& response) {
            PostResponse(response);
        }
    );
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
    std::lock_guard lock(queue_mutex_);
    cancelled_requests_.insert(id);
}

bool KSMWebViewHost::IsCancelled(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    std::lock_guard lock(queue_mutex_);
    return cancelled_requests_.find(id) != cancelled_requests_.end();
}

void KSMWebViewHost::ClearCancelled(const std::string& id) {
    if (id.empty()) {
        return;
    }
    std::lock_guard lock(queue_mutex_);
    cancelled_requests_.erase(id);
}

void KSMWebViewHost::WorkerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(queue_mutex_);
            queue_ready_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });
            if (stopping_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
    }
}

void KSMWebViewHost::NotifyWebViewReloaded() {
    router_.EmitEvent(
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
