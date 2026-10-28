#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
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
    static constexpr std::size_t kMaxConcurrentCommands = 8;

    void WorkerLoop();
    void PostResponse(const std::string& message);
    void CancelRequest(const std::string& id);
    bool IsCancelled(const std::string& id);
    void ClearCancelled(const std::string& id);

    SendMessage send_message_;
    Dispatch dispatch_;
    KSMRouter router_;
    std::mutex queue_mutex_;
    std::condition_variable queue_ready_;
    std::deque<std::function<void()>> tasks_;
    std::unordered_set<std::string> cancelled_requests_;
    std::size_t in_flight_ = 0;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};
