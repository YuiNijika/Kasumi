#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>

class KSMFileSystem;
class KSMHostServices;

class KSMRouter {
public:
    using ResponseSender = std::function<void(const std::string&)>;

    explicit KSMRouter(
        std::shared_ptr<KSMFileSystem> file_system = nullptr,
        std::shared_ptr<KSMHostServices> host_services = nullptr
    );
    void HandleMessage(const std::string& message, const ResponseSender& send_response);
    void EmitEvent(
        const std::string& event,
        const std::string& payload_json,
        const ResponseSender& send_response
    ) const;

private:
    bool HasPermission(const std::string& permission) const;

    std::shared_ptr<KSMFileSystem> file_system_;
    std::shared_ptr<KSMHostServices> host_services_;
    mutable std::mutex state_mutex_;
    std::set<std::string> granted_permissions_;
    std::set<std::string> subscribed_events_;
};
