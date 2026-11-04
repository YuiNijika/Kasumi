#include "ksm_os_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

void RequirePermission(const KSMPluginContext& context) {
    if (context.has_permission && context.has_permission("os.info")) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

} // namespace

const std::string& KSMOperatingSystemPlugin::Id() const {
    static const std::string id = "os";
    return id;
}

const std::vector<std::string>& KSMOperatingSystemPlugin::Commands() const {
    static const std::vector<std::string> commands = {"os.getInfo"};
    return commands;
}

const std::vector<std::string>& KSMOperatingSystemPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {"os.info"};
    return permissions;
}

nlohmann::json KSMOperatingSystemPlugin::Handle(
    const std::string& command,
    const nlohmann::json& args,
    const KSMPluginContext& context
) {
    (void)command;
    (void)args;
    RequirePermission(context);
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The operating system service is not configured."
        );
    }
    const auto info = context.host_services->GetOperatingSystemInfo();
    if (!info) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "Operating system information is unavailable."
        );
    }
    return nlohmann::json{
        {"platform", info->platform},
        {"architecture", info->architecture},
        {"version", info->version},
        {"locale", info->locale},
        {"timezone", info->timezone},
        {"appVersion", context.host_services->AppVersion()},
        {"devMode", info->dev_mode}
    };
}
