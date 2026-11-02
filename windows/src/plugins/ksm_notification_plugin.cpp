#include "ksm_notification_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

}

const std::string& KSMNotificationPlugin::Id() const {
    static const std::string id = "notification";
    return id;
}

const std::vector<std::string>& KSMNotificationPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "notification.show"
    };
    return commands;
}

const std::vector<std::string>& KSMNotificationPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "notification.show"
    };
    return permissions;
}

Json KSMNotificationPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    (void)command;
    if (
        !context.has_permission ||
        !context.has_permission("notification.show")
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The requested permission was denied."
        );
    }
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The notification service is not configured."
        );
    }
    if (
        !args.contains("title") ||
        !args.at("title").is_string() ||
        !args.contains("body") ||
        !args.at("body").is_string()
    ) {
        throw KSMHostException(
            "INVALID_ARGS",
            "title and body are required."
        );
    }
    return Json{{
        "shown",
        context.host_services->ShowNotification(
            args.at("title").get_ref<const std::string&>(),
            args.at("body").get_ref<const std::string&>()
        )
    }};
}
