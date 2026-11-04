#include "ksm_shell_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"
#include "../platform/ksm_url_policy.h"
#include "../commands/ksm_filesystem.h"
#include "../platform/ksm_virtual_path.h"

namespace {

using Json = nlohmann::json;

}

const std::string& KSMShellPlugin::Id() const {
    static const std::string id = "shell";
    return id;
}

const std::vector<std::string>& KSMShellPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "shell.openExternal",
        "shell.openPath",
        "shell.showInFolder"
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
    if (command != "shell.openExternal") {
        const std::string& path = args.at("path").get_ref<const std::string&>();
        if (!KSMVirtualPath::IsSafe(path)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the sandbox."
            );
        }
        if (!context.file_system) {
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            );
        }
        const auto resolved = context.file_system->ResolveReadableFile(path);
        if (!resolved) {
            throw KSMHostException("NOT_FOUND", "The path was not found.");
        }
        return Json{{
            command == "shell.showInFolder" ? "shown" : "opened",
            command == "shell.showInFolder"
                ? context.host_services->ShowInFolder(resolved->string())
                : context.host_services->OpenPath(resolved->string())
        }};
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
