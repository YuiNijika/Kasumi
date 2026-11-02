#include "ksm_http_plugin.h"

#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"
#include "../platform/ksm_url_policy.h"
#include "../platform/ksm_virtual_path.h"

namespace {

using Json = nlohmann::json;

}

const std::string& KSMHttpPlugin::Id() const {
    static const std::string id = "http";
    return id;
}

const std::vector<std::string>& KSMHttpPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "http.download"
    };
    return commands;
}

const std::vector<std::string>& KSMHttpPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "http.download"
    };
    return permissions;
}

Json KSMHttpPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    (void)command;
    if (!context.has_permission || !context.has_permission("http.download")) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The requested permission was denied."
        );
    }
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The download service is not configured."
        );
    }
    if (
        !args.contains("url") ||
        !args.at("url").is_string() ||
        !args.contains("path") ||
        !args.at("path").is_string()
    ) {
        throw KSMHostException(
            "INVALID_ARGS",
            "url and path are required."
        );
    }
    const std::string& url = args.at("url").get_ref<const std::string&>();
    const std::string& path = args.at("path").get_ref<const std::string&>();
    if (!KSMUrlPolicy::ParseHttpUrl(url, true)) {
        throw KSMHostException("INVALID_ARGS", "url must use HTTPS.");
    }
    if (
        !KSMVirtualPath::IsSafe(path) ||
        KSMVirtualPath::IsReadOnly(path) ||
        KSMVirtualPath::IsPicked(path)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "path is outside the writable sandbox."
        );
    }
    if (
        args.contains("overwrite") &&
        !args.at("overwrite").is_boolean()
    ) {
        throw KSMHostException(
            "INVALID_ARGS",
            "overwrite must be a boolean."
        );
    }
    const auto result = context.host_services->Download(
        url,
        path,
        args.value("overwrite", false),
        context.request_id
    );
    if (!result) {
        throw KSMHostException(
            "IO_ERROR",
            "The download could not be completed."
        );
    }
    return Json{{"path", result->path}, {"bytes", result->bytes}};
}
