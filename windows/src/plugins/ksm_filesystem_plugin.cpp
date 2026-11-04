#include "ksm_filesystem_plugin.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "../platform/ksm_host_services.h"
#include "../platform/ksm_virtual_path.h"

namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxInlineTextBytes = 64 * 1024;
constexpr std::size_t kMaxBinaryBytes = 32 * 1024 * 1024;

constexpr char kBase64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EncodeBase64(const std::vector<std::uint8_t> &bytes) {
  if (bytes.empty()) {
    return "";
  }
  std::string output;
  output.reserve(((bytes.size() + 2) / 3) * 4);
  for (std::size_t index = 0; index < bytes.size(); index += 3) {
    const std::uint32_t value =
        static_cast<std::uint32_t>(bytes[index]) << 16 |
        (index + 1 < bytes.size() ? static_cast<std::uint32_t>(bytes[index + 1]) << 8 : 0) |
        (index + 2 < bytes.size() ? static_cast<std::uint32_t>(bytes[index + 2]) : 0);
    output.push_back(kBase64[(value >> 18) & 0x3F]);
    output.push_back(kBase64[(value >> 12) & 0x3F]);
    output.push_back(index + 1 < bytes.size() ? kBase64[(value >> 6) & 0x3F] : '=');
    output.push_back(index + 2 < bytes.size() ? kBase64[value & 0x3F] : '=');
  }
  return output;
}

std::optional<std::vector<std::uint8_t>> DecodeBase64(const std::string &text) {
  std::vector<int> values(256, -1);
  for (int index = 0; index < 64; ++index) {
    values[static_cast<unsigned char>(kBase64[index])] = index;
  }
  if (text.size() % 4 != 0) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> output;
  output.reserve((text.size() / 4) * 3);
  for (std::size_t index = 0; index < text.size(); index += 4) {
    if (index + 3 >= text.size()) {
      return std::nullopt;
    }
    const bool c_padding = text[index + 2] == '=';
    const bool d_padding = text[index + 3] == '=';
    if (c_padding != d_padding || text[index] == '=' || text[index + 1] == '=') {
      return std::nullopt;
    }
    const int a = values[static_cast<unsigned char>(text[index])];
    const int b = values[static_cast<unsigned char>(text[index + 1])];
    const int c_value =
        c_padding ? 0 : values[static_cast<unsigned char>(text[index + 2])];
    const int d =
        d_padding ? 0 : values[static_cast<unsigned char>(text[index + 3])];
    if (a < 0 || b < 0 || c_value < 0 || d < 0) {
      return std::nullopt;
    }
    const std::uint32_t value =
        static_cast<std::uint32_t>(a) << 18 |
        static_cast<std::uint32_t>(b) << 12 |
        static_cast<std::uint32_t>(c_value) << 6 |
        static_cast<std::uint32_t>(d);
    if (!c_padding) {
      output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
      output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    } else {
      output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    }
    if (!d_padding) {
      output.push_back(static_cast<std::uint8_t>(value & 0xFF));
    }
  }
  return output;
}

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

Json AppendText(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (
        !KSMVirtualPath::IsSafe(path) ||
        KSMVirtualPath::IsReadOnly(path) ||
        KSMVirtualPath::IsPicked(path)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the writable sandbox."
        );
    }
    const std::string& content =
        args.at("content").get_ref<const std::string&>();
    if (!context.file_system->AppendText(path, content)) {
        throw KSMHostException("IO_ERROR", "The file could not be appended.");
    }
    return Json{{"written", true}};
}

Json Copy(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& from = args.at("from").get_ref<const std::string&>();
    const std::string& to = args.at("to").get_ref<const std::string&>();
    if (!KSMVirtualPath::IsSafe(from) || !KSMVirtualPath::IsSafe(to)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    if (KSMVirtualPath::IsReadOnly(to) || KSMVirtualPath::IsPicked(to)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The target path is outside the writable sandbox."
        );
    }
    if (!context.file_system->Copy(from, to)) {
        throw KSMHostException("IO_ERROR", "The file could not be copied.");
    }
    return Json{{"copied", true}};
}

Json MoveFile(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& from = args.at("from").get_ref<const std::string&>();
    const std::string& to = args.at("to").get_ref<const std::string&>();
    if (
        !KSMVirtualPath::IsSafe(from) ||
        !KSMVirtualPath::IsSafe(to) ||
        KSMVirtualPath::IsReadOnly(from) ||
        KSMVirtualPath::IsReadOnly(to) ||
        KSMVirtualPath::IsPicked(from) ||
        KSMVirtualPath::IsPicked(to)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the writable sandbox."
        );
    }
    if (!context.file_system->Move(from, to)) {
        throw KSMHostException("IO_ERROR", "The file could not be moved.");
    }
    return Json{{"moved", true}};
}

Json ReadBytes(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    if (!KSMVirtualPath::IsSafe(path) || KSMVirtualPath::IsPicked(path)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    const auto bytes = context.file_system->ReadBytes(path);
    if (!bytes) {
        throw KSMHostException("NOT_FOUND", "The file was not found.");
    }
    if (bytes->size() > kMaxBinaryBytes) {
        throw KSMHostException(
            "RESOURCE_LIMIT",
            "The binary result exceeds the configured limit."
        );
    }
    return Json{{"base64", EncodeBase64(*bytes)}};
}

Json WriteBytes(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& path = PathArg(args);
    const std::string& base64 =
        args.at("base64").get_ref<const std::string&>();
    if (
        !KSMVirtualPath::IsSafe(path) ||
        KSMVirtualPath::IsReadOnly(path) ||
        KSMVirtualPath::IsPicked(path)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the writable sandbox."
        );
    }
    const auto bytes = DecodeBase64(base64);
    if (!bytes) {
        throw KSMHostException("INVALID_ARGS", "base64 is not valid.");
    }
    if (bytes->size() > kMaxBinaryBytes) {
        throw KSMHostException(
            "RESOURCE_LIMIT",
            "The binary payload exceeds the configured limit."
        );
    }
    if (!context.file_system->WriteBytes(path, *bytes)) {
        throw KSMHostException("IO_ERROR", "The file could not be written.");
    }
    return Json{{"written", true}};
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
        "fs.appendText",
        "fs.copy",
        "fs.move",
        "fs.readBytes",
        "fs.writeBytes",
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
    if (command == "fs.appendText") {
        RequirePermission(context, "fs.write");
        return AppendText(args, context);
    }
    if (command == "fs.copy") {
        RequirePermission(context, "fs.read");
        RequirePermission(context, "fs.write");
        return Copy(args, context);
    }
    if (command == "fs.move") {
        RequirePermission(context, "fs.write");
        return MoveFile(args, context);
    }
    if (command == "fs.readBytes") {
        RequirePermission(context, "fs.read");
        return ReadBytes(args, context);
    }
    if (command == "fs.writeBytes") {
        RequirePermission(context, "fs.write");
        return WriteBytes(args, context);
    }
    if (command == "fs.exists") {
        RequirePermission(context, "fs.read");
        return Exists(args, context);
    }
    RequirePermission(context, "fs.remove");
    return Remove(args, context);
}
