#include "ksm_log_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

void RequirePermission(const KSMPluginContext& context) {
    if (context.has_permission && context.has_permission("log.write")) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

const std::string& StringArg(
    const nlohmann::json& args,
    const char* field
) {
    if (!args.contains(field) || !args.at(field).is_string()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " is required."
        );
    }
    return args.at(field).get_ref<const std::string&>();
}

} // namespace

const std::string& KSMLogPlugin::Id() const {
    static const std::string id = "log";
    return id;
}

const std::vector<std::string>& KSMLogPlugin::Commands() const {
    static const std::vector<std::string> commands = {"log.write"};
    return commands;
}

const std::vector<std::string>& KSMLogPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {"log.write"};
    return permissions;
}

nlohmann::json KSMLogPlugin::Handle(
    const std::string& command,
    const nlohmann::json& args,
    const KSMPluginContext& context
) {
    (void)command;
    RequirePermission(context);
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The logger service is not configured."
        );
    }
    const std::string& level = StringArg(args, "level");
    const std::string& message = StringArg(args, "message");
    std::string target;
    if (args.contains("target")) {
        target = StringArg(args, "target");
    }
    if (!context.host_services->WriteLog(level, target, message)) {
        throw KSMHostException("IO_ERROR", "The message could not be logged.");
    }
    return nlohmann::json{{"logged", true}};
}
