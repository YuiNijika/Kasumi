#include "ksm_dialog_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

void RequirePermission(const KSMPluginContext& context) {
    if (context.has_permission && context.has_permission("dialog")) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

Json ShowMessage(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& title = args.at("title").get_ref<const std::string&>();
    const std::string& message =
        args.at("message").get_ref<const std::string&>();
    const bool confirmed = command == "dialog.message"
        ? context.host_services->ShowMessage(title, message)
        : context.host_services->Confirm(title, message);
    return Json{{"confirmed", confirmed}};
}

Json OpenFile(
    const Json& args,
    const KSMPluginContext& context
) {
    std::vector<std::string> filters;
    if (args.contains("filters")) {
        for (const auto& filter : args.at("filters")) {
            filters.push_back(filter.get<std::string>());
        }
    }
    const auto file = context.host_services->OpenFile(filters);
    return Json{{"file", file ? Json(*file) : Json(nullptr)}};
}

Json SaveFile(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string suggested_name = args.value("suggestedName", "");
    const auto file = context.host_services->SaveFile(suggested_name);
    return Json{{"file", file ? Json(*file) : Json(nullptr)}};
}

}

const std::string& KSMDialogPlugin::Id() const {
    static const std::string id = "dialog";
    return id;
}

const std::vector<std::string>& KSMDialogPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile"
    };
    return commands;
}

const std::vector<std::string>& KSMDialogPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {"dialog"};
    return permissions;
}

Json KSMDialogPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The dialog service is not configured."
        );
    }
    RequirePermission(context);

    if (command == "dialog.message" || command == "dialog.confirm") {
        if (
            !args.contains("title") ||
            !args.at("title").is_string() ||
            !args.contains("message") ||
            !args.at("message").is_string()
        ) {
            throw KSMHostException(
                "INVALID_ARGS",
                "title and message are required."
            );
        }
        return ShowMessage(command, args, context);
    }
    if (command == "dialog.openFile") {
        return OpenFile(args, context);
    }
    return SaveFile(args, context);
}
