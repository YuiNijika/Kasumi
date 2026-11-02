#include "ksm_filesystem_plugin.h"

#include <cstddef>
#include <string>

#include "../commands/ksm_filesystem.h"
#include "../platform/ksm_host_services.h"
#include "../platform/ksm_virtual_path.h"

namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxInlineTextBytes = 64 * 1024;

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

const std::string& PathArg(const Json& args) {
    return args.at("path").get_ref<const std::string&>();
}

Json MetadataJson(const KSMFileMetadata& metadata) {
    Json modified_at = nullptr;
    if (metadata.modified_at_ms) {
        modified_at = *metadata.modified_at_ms;
    }
    return Json{
        {"kind", metadata.kind == KSMFileKind::Directory ? "directory" : "file"},
        {"size", metadata.size},
        {"modifiedAt", modified_at}
    };
}

Json DirectoryEntryJson(const KSMDirectoryEntry& entry) {
    Json value = MetadataJson(entry.metadata);
    value["name"] = entry.name;
    value["path"] = entry.path;
    return value;
}

Json MakeDirectory(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (
        !KSMVirtualPath::IsSafeDirectory(path) ||
        KSMVirtualPath::IsPicked(path) ||
        KSMVirtualPath::IsReadOnly(path)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the writable sandbox."
        );
    }
    const bool recursive = args.value("recursive", false);
    const auto created = context.file_system->MakeDirectory(path, recursive);
    if (!created) {
        throw KSMHostException(
            "IO_ERROR",
            "The directory could not be created."
        );
    }
    return Json{{"created", *created}};
}

Json ReadDirectory(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (
        !KSMVirtualPath::IsSafeDirectory(path) ||
        KSMVirtualPath::IsPicked(path)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is not a readable directory."
        );
    }
    const auto entries = context.file_system->ReadDir(path);
    if (!entries) {
        const auto metadata = context.file_system->Stat(path);
        if (metadata && metadata->kind != KSMFileKind::Directory) {
            throw KSMHostException(
                "INVALID_ARGS",
                "The path is not a directory."
            );
        }
        throw KSMHostException(
            metadata ? "IO_ERROR" : "NOT_FOUND",
            metadata
                ? "The directory could not be read."
                : "The directory was not found."
        );
    }
    Json values = Json::array();
    for (const auto& entry : *entries) {
        values.push_back(DirectoryEntryJson(entry));
    }
    return Json{{"entries", values}};
}

Json Stat(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (!KSMVirtualPath::IsSafeStat(path) || path == "app://picked") {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    if (KSMVirtualPath::IsPicked(path)) {
        if (!context.host_services) {
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The picked file service is not configured."
            );
        }
        const auto metadata = context.host_services->PickedStat(path);
        if (!metadata) {
            throw KSMHostException("NOT_FOUND", "The file was not found.");
        }
        return MetadataJson(*metadata);
    }
    const auto metadata = context.file_system->Stat(path);
    if (!metadata) {
        throw KSMHostException("NOT_FOUND", "The path was not found.");
    }
    return MetadataJson(*metadata);
}

Json ReadText(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (!KSMVirtualPath::IsSafe(path)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    const bool is_picked = KSMVirtualPath::IsPicked(path);
    if (is_picked && !context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The picked file service is not configured."
        );
    }
    const auto value = is_picked
        ? context.host_services->ReadPickedText(path)
        : context.file_system->ReadText(path);
    if (!value) {
        const bool exists = is_picked
            ? context.host_services->PickedExists(path)
            : context.file_system->Exists(path);
        throw KSMHostException(
            exists ? "IO_ERROR" : "NOT_FOUND",
            exists ? "The file could not be read." : "The file was not found."
        );
    }
    if (value->size() <= kMaxInlineTextBytes) {
        return Json(*value);
    }
    if (path.starts_with("app://tmp/read-")) {
        return Json{{"file", path}};
    }
    const auto temporary = context.file_system->CreateTemporaryText(*value);
    if (!temporary) {
        throw KSMHostException(
            "IO_ERROR",
            "The large text result could not be staged."
        );
    }
    return Json{{"file", *temporary}};
}

Json WriteText(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (!KSMVirtualPath::IsSafe(path)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    if (KSMVirtualPath::IsReadOnly(path)) {
        throw KSMHostException("PERMISSION_DENIED", "The path is read-only.");
    }
    const bool is_picked = KSMVirtualPath::IsPicked(path);
    if (is_picked && !context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The picked file service is not configured."
        );
    }
    const std::string& content =
        args.at("content").get_ref<const std::string&>();
    const bool written = is_picked
        ? context.host_services->WritePickedText(path, content)
        : context.file_system->WriteText(path, content);
    if (!written) {
        throw KSMHostException("IO_ERROR", "The file could not be written.");
    }
    return Json{{"written", true}};
}

Json Exists(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (!KSMVirtualPath::IsSafe(path)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    const bool is_picked = KSMVirtualPath::IsPicked(path);
    if (is_picked && !context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The picked file service is not configured."
        );
    }
    return Json{{
        "exists",
        is_picked
            ? context.host_services->PickedExists(path)
            : context.file_system->Exists(path)
    }};
}

Json Remove(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (!KSMVirtualPath::IsSafe(path)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    if (KSMVirtualPath::IsReadOnly(path)) {
        throw KSMHostException("PERMISSION_DENIED", "The path is read-only.");
    }
    const bool is_picked = KSMVirtualPath::IsPicked(path);
    if (is_picked && !context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The picked file service is not configured."
        );
    }
    return Json{{
        "removed",
        is_picked
            ? context.host_services->RemovePicked(path)
            : context.file_system->Remove(path)
    }};
}

}

const std::string& KSMFileSystemPlugin::Id() const {
    static const std::string id = "fs";
    return id;
}

const std::vector<std::string>& KSMFileSystemPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat"
    };
    return commands;
}

const std::vector<std::string>& KSMFileSystemPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "fs.read",
        "fs.write",
        "fs.remove"
    };
    return permissions;
}

Json KSMFileSystemPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    if (!context.file_system) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The filesystem is not configured."
        );
    }
    if (
        !args.contains("path") ||
        !args.at("path").is_string() ||
        args.at("path").get_ref<const std::string&>().empty()
    ) {
        throw KSMHostException("INVALID_ARGS", "path is required.");
    }

    if (command == "fs.mkdir") {
        RequirePermission(context, "fs.write");
        return MakeDirectory(args, context);
    }
    if (command == "fs.readDir") {
        RequirePermission(context, "fs.read");
        return ReadDirectory(args, context);
    }
    if (command == "fs.stat") {
        RequirePermission(context, "fs.read");
        return Stat(args, context);
    }
    if (command == "fs.readText") {
        RequirePermission(context, "fs.read");
        return ReadText(args, context);
    }
    if (command == "fs.writeText") {
        RequirePermission(context, "fs.write");
        return WriteText(args, context);
    }
    if (command == "fs.exists") {
        RequirePermission(context, "fs.read");
        return Exists(args, context);
    }
    RequirePermission(context, "fs.remove");
    return Remove(args, context);
}
