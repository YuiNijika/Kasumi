#include "ksm_clipboard_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

void RequirePermission(
    const KSMPluginContext& context,
    const std::string& permission
) {
    if (context.has_permission && context.has_permission(permission)) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

std::shared_ptr<KSMHostServices> RequireHost(
    const KSMPluginContext& context
) {
    if (context.host_services) {
        return context.host_services;
    }
    throw KSMHostException(
        "NOT_SUPPORTED",
        "The clipboard service is not configured."
    );
}

}

const std::string& KSMClipboardPlugin::Id() const {
    static const std::string id = "clipboard";
    return id;
}

const std::vector<std::string>& KSMClipboardPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "clipboard.readText",
        "clipboard.writeText"
    };
    return commands;
}

const std::vector<std::string>& KSMClipboardPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "clipboard.read",
        "clipboard.write"
    };
    return permissions;
}

Json KSMClipboardPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    const auto host = RequireHost(context);
    if (command == "clipboard.readText") {
        RequirePermission(context, "clipboard.read");
        return Json{{"text", host->ReadClipboardText().value_or("")}};
    }

    RequirePermission(context, "clipboard.write");
    if (!args.contains("text") || !args.at("text").is_string()) {
        throw KSMHostException("INVALID_ARGS", "text is required.");
    }
    return Json{{
        "written",
        host->WriteClipboardText(
            args.at("text").get_ref<const std::string&>()
        )
    }};
}
