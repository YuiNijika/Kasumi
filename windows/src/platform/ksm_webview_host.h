#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../bridge/ksm_router.h"

class KSMFileSystem;
class KSMHostServices;

class KSMWebViewHost {
public:
    using SendMessage = std::function<void(const std::string&)>;
    using Dispatch = std::function<void(std::function<void()>)>;

    explicit KSMWebViewHost(
        SendMessage send_message,
        std::shared_ptr<KSMFileSystem> file_system = nullptr,
        std::shared_ptr<KSMHostServices> host_services = nullptr,
        Dispatch dispatch = {}
    );
    ~KSMWebViewHost();

    void OnMessageReceived(const std::string& message);
    bool OnMessageReceivedFromOrigin(
        const std::string& message,
        const std::string& origin
    );
    void NotifyWebViewReloaded();
    void NotifyWindowResized(int width, int height);
    bool IsAllowedOrigin(const std::string& origin) const;
    bool IsAllowedNavigation(const std::string& url) const;

private:
    struct RequestState {
        std::string id;
        std::atomic<bool> completed{false};
    };

    struct Deadline {
        std::chrono::steady_clock::time_point at;
        std::shared_ptr<RequestState> state;
    };

    static constexpr std::size_t kMaxConcurrentCommands = 8;
    static constexpr std::size_t kMaxQueuedCommands = 32;

    void WorkerLoop();
    void TimerLoop();
    void PostResponse(const std::string& message);
    void CancelRequest(const std::string& id);
    void RegisterRequest(
        const std::shared_ptr<RequestState>& state,
        long long timeout_ms
    );
    void UnregisterRequest(const std::shared_ptr<RequestState>& state);
    void RespondOnce(
        const std::shared_ptr<RequestState>& state,
        const std::string& response
    );

    SendMessage send_message_;
    Dispatch dispatch_;
    KSMRouter router_;
    std::mutex queue_mutex_;
    std::condition_variable queue_ready_;
    std::deque<std::function<void()>> tasks_;
    std::unordered_map<std::string, std::shared_ptr<RequestState>> active_requests_;
    std::size_t in_flight_ = 0;
    bool stopping_ = false;
    std::vector<std::thread> workers_;

    std::mutex timer_mutex_;
    std::condition_variable timer_ready_;
    std::deque<Deadline> deadlines_;
    bool timer_stopping_ = false;
    std::thread timer_thread_;
};
