#include "ksm_shell_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"
#include "../platform/ksm_url_policy.h"

namespace {

using Json = nlohmann::json;

}

const std::string& KSMShellPlugin::Id() const {
    static const std::string id = "shell";
    return id;
}

const std::vector<std::string>& KSMShellPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "shell.openExternal"
    };
    return commands;
}

const std::vector<std::string>& KSMShellPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "shell.open"
    };
    return permissions;
}

Json KSMShellPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    (void)command;
    if (!context.has_permission || !context.has_permission("shell.open")) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The requested permission was denied."
        );
    }
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The shell service is not configured."
        );
    }
    if (!args.contains("url") || !args.at("url").is_string()) {
        throw KSMHostException("INVALID_ARGS", "url is required.");
    }
    const std::string& url = args.at("url").get_ref<const std::string&>();
    if (!KSMUrlPolicy::ParseHttpUrl(url)) {
        throw KSMHostException(
            "INVALID_ARGS",
            "url must be an absolute HTTP or HTTPS URL."
        );
    }
    if (!context.host_services->IsExternalUrlAllowed(url)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The URL origin is not allowlisted."
        );
    }
    return Json{{"opened", context.host_services->OpenExternal(url)}};
}
