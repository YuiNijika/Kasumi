#include "ksm_window_plugin.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

constexpr std::int64_t kMinWindowWidth = 320;
constexpr std::int64_t kMinWindowHeight = 240;

std::shared_ptr<KSMHostServices> RequireHost(
    const KSMPluginContext& context
) {
    if (context.host_services) {
        return context.host_services;
    }
    throw KSMHostException(
        "NOT_SUPPORTED",
        "The window service is not configured."
    );
}

void RequirePermission(const KSMPluginContext& context) {
    if (context.has_permission && context.has_permission("window.control")) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

std::int64_t IntegerValue(const Json& value, const char* field) {
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()
            )) {
            throw KSMHostException(
                "INVALID_ARGS",
                std::string(field) + " is out of range."
            );
        }
        return static_cast<std::int64_t>(number);
    }
    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }
    throw KSMHostException(
        "INVALID_ARGS",
        std::string(field) + " must be an integer."
    );
}

KSMWindowSize WindowSizeArg(const Json& args) {
    if (!args.contains("width") || !args.contains("height")) {
        throw KSMHostException(
            "INVALID_ARGS",
            "width and height are required."
        );
    }
    const std::int64_t width = IntegerValue(args.at("width"), "width");
    const std::int64_t height = IntegerValue(args.at("height"), "height");
    if (
        width < kMinWindowWidth ||
        height < kMinWindowHeight ||
        width > std::numeric_limits<int>::max() ||
        height > std::numeric_limits<int>::max()
    ) {
        throw KSMHostException(
            "INVALID_ARGS",
            "width must be at least 320 and height at least 240."
        );
    }
    return KSMWindowSize{
        static_cast<int>(width),
        static_cast<int>(height)
    };
}

}

const std::string& KSMWindowPlugin::Id() const {
    static const std::string id = "window";
    return id;
}

const std::vector<std::string>& KSMWindowPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "window.setTitle",
        "window.minimize",
        "window.maximize",
        "window.setSize",
        "window.close"
    };
    return commands;
}

const std::vector<std::string>& KSMWindowPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "window.control"
    };
    return permissions;
}

Json KSMWindowPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequirePermission(context);
    const auto host = RequireHost(context);

    if (command == "window.setTitle") {
        if (!args.contains("title") || !args.at("title").is_string()) {
            throw KSMHostException("INVALID_ARGS", "title is required.");
        }
        return Json{{
            "updated",
            host->SetWindowTitle(
                args.at("title").get_ref<const std::string&>()
            )
        }};
    }
    if (command == "window.minimize") {
        return Json{{"updated", host->MinimizeWindow()}};
    }
    if (command == "window.maximize") {
        return Json{{"updated", host->MaximizeWindow()}};
    }
    if (command == "window.setSize") {
        return Json{{"updated", host->SetWindowSize(WindowSizeArg(args))}};
    }
    return Json{{"closed", host->CloseWindow()}};
}
