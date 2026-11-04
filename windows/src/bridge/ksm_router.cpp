#include "ksm_router.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "../commands/ksm_phase1.h"
#include "../generated/contract.h"
#include "../platform/ksm_host_services.h"
#include "../platform/ksm_url_policy.h"
#include "../platform/ksm_virtual_path.h"
#include "../plugins/ksm_dialog_plugin.h"
#include "../plugins/ksm_filesystem_plugin.h"
#include "../plugins/ksm_sql_plugin.h"
#include "ksm_version.h"
#include "ksm_command_registry.h"

namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxInlineTextBytes = 64 * 1024;

void SendError(
    const Json& id,
    const std::string& code,
    const std::string& message,
    const KSMRouter::ResponseSender& send_response
) {
    send_response(Json{
        {"type", "response"},
        {"id", id},
        {"ok", false},
        {"error", {{"code", code}, {"message", message}}}
    }.dump());
}

void SendSuccess(
    const Json& id,
    const Json& data,
    const KSMRouter::ResponseSender& send_response
) {
    send_response(Json{
        {"type", "response"},
        {"id", id},
        {"ok", true},
        {"data", data}
    }.dump());
}

bool IsString(const Json& args, const char* field) {
    return args.contains(field) && args.at(field).is_string();
}

const char* FileKindName(KSMFileKind kind) {
    return kind == KSMFileKind::Directory ? "directory" : "file";
}

Json MetadataJson(const KSMFileMetadata& metadata) {
    Json modified_at = nullptr;
    if (metadata.modified_at_ms) {
        modified_at = *metadata.modified_at_ms;
    }
    return Json{
        {"kind", FileKindName(metadata.kind)},
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

KSMSqlValue SqlValueFromJson(const Json& value) {
    if (value.is_null()) {
        return nullptr;
    }
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()
            )) {
            throw KSMHostException(
                "INVALID_ARGS",
                "SQL integer parameters must fit in a signed 64-bit integer."
            );
        }
        return static_cast<std::int64_t>(number);
    }
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        return value.get<std::string>();
    }
    throw KSMHostException(
        "INVALID_ARGS",
        "SQL parameters must be strings, numbers, booleans or null."
    );
}

Json SqlValueToJson(const KSMSqlValue& value) {
    return std::visit(
        [](const auto& item) -> Json {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return nullptr;
            } else {
                return item;
            }
        },
        value
    );
}

bool Contains(
    const std::vector<std::string>& values,
    const std::string& value
) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

void AppendUnique(
    std::vector<std::string>& values,
    const std::string& value
) {
    if (!Contains(values, value)) {
        values.push_back(value);
    }
}

std::vector<std::shared_ptr<KSMPlugin>> BuildPlugins(
    std::vector<std::shared_ptr<KSMPlugin>> additional
) {
    const auto& generated_ids = KSMGeneratedContract::Plugins();
    std::set<std::string> replaceable_ids;
    for (const auto& plugin_id : generated_ids) {
        if (plugin_id != "core") {
            replaceable_ids.insert(plugin_id);
        }
    }
    std::set<std::string> seen;
    std::set<std::string> overridden_ids;
    for (const auto& plugin : additional) {
        if (!plugin) {
            throw std::invalid_argument("A Kasumi plugin cannot be null.");
        }
        if (
            !seen.insert(plugin->Id()).second ||
            (
                Contains(generated_ids, plugin->Id()) &&
                !replaceable_ids.contains(plugin->Id())
            )
        ) {
            throw std::invalid_argument(
                "The custom Kasumi plugin ID conflicts with a built-in plugin."
            );
        }
        const auto& commands = plugin->Commands();
        if (replaceable_ids.contains(plugin->Id())) {
            const auto& generated_commands = KSMGeneratedContract::PluginCommands()
                .at(plugin->Id());
            const std::set<std::string> expected(
                generated_commands.begin(),
                generated_commands.end()
            );
            const std::set<std::string> actual(
                commands.begin(),
                commands.end()
            );
            if (actual != expected) {
                throw std::invalid_argument(
                    "A replacement built-in plugin must own exactly the "
                    "generated commands for its plugin ID."
                );
            }
        } else {
            for (const auto& command : commands) {
                if (Contains(KSMGeneratedContract::Commands(), command)) {
                    throw std::invalid_argument(
                        "A custom Kasumi plugin cannot take ownership of a "
                        "generated command."
                    );
                }
            }
        }
        overridden_ids.insert(plugin->Id());
    }

    auto plugins = KSMDefaultPlugins();
    plugins.erase(
        std::remove_if(
            plugins.begin(),
            plugins.end(),
            [&overridden_ids](const std::shared_ptr<KSMPlugin>& plugin) {
                return overridden_ids.contains(plugin->Id());
            }
        ),
        plugins.end()
    );
    plugins.insert(
        plugins.end(),
        std::make_move_iterator(additional.begin()),
        std::make_move_iterator(additional.end())
    );
    return plugins;
}

}

KSMRouter::KSMRouter(
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services,
    std::vector<std::string> allowed_commands,
    std::vector<std::shared_ptr<KSMPlugin>> plugins
)
    : KSMRouter(
          std::move(file_system),
          std::move(host_services),
          std::move(allowed_commands),
          {},
          std::move(plugins)
      ) {}

KSMRouter::KSMRouter(
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services,
    std::vector<std::string> allowed_commands,
    std::vector<std::string> allowed_permissions,
    std::vector<std::shared_ptr<KSMPlugin>> plugins
)
    : file_system_(std::move(file_system)),
      host_services_(std::move(host_services)),
      plugin_registry_(BuildPlugins(std::move(plugins))) {
    supported_commands_ = KSMCommandRegistry::SupportedCommands(
        allowed_commands
    );
    const std::set<std::string> requested(
        allowed_commands.begin(),
        allowed_commands.end()
    );
    for (const auto& command : plugin_registry_.Commands()) {
        if (
            Contains(KSMGeneratedContract::Commands(), command) &&
            !Contains(KSMGeneratedContract::WindowsCommands(), command)
        ) {
            continue;
        }
        if (requested.contains(command)) {
            AppendUnique(supported_commands_, command);
        }
    }
    const std::set<std::string> available_permissions(
        KSMGeneratedContract::Permissions().begin(),
        KSMGeneratedContract::Permissions().end()
    );
    const auto& plugin_permissions = plugin_registry_.Permissions();
    for (const auto& permission : allowed_permissions) {
        if (
            available_permissions.contains(permission) ||
            std::find(
                plugin_permissions.begin(),
                plugin_permissions.end(),
                permission
            ) != plugin_permissions.end()
        ) {
            granted_permissions_.insert(permission);
        }
    }
    const auto& command_permissions =
        KSMGeneratedContract::CommandPermissions();
    for (const auto& command : supported_commands_) {
        const bool is_generated = Contains(
            KSMGeneratedContract::Commands(),
            command
        );
        std::vector<std::string> required_permissions;
        if (
            is_generated &&
            command_permissions.contains(command) &&
            command_permissions.at(command).is_array()
        ) {
            required_permissions = command_permissions.at(command)
                .get<std::vector<std::string>>();
        } else {
            required_permissions =
                plugin_registry_.PermissionsForCommand(command);
        }
        const bool permitted = std::all_of(
            required_permissions.begin(),
            required_permissions.end(),
            [this](const std::string& permission) {
                return granted_permissions_.contains(permission);
            }
        );
        if (!permitted) {
            continue;
        }
        capability_commands_.push_back(command);
        const std::string plugin_id =
            plugin_registry_.PluginIdForCommand(command);
        AppendUnique(
            plugin_ids_,
            plugin_id.empty() ? "core" : plugin_id
        );
    }
}

const std::vector<std::string>& KSMRouter::SupportedCommands() const {
    return supported_commands_;
}

void KSMRouter::CancelRequest(const std::string& request_id) {
    if (host_services_) {
        host_services_->CancelRequest(request_id);
    }
}

bool KSMRouter::HasPermission(const std::string& permission) const {
    std::lock_guard lock(state_mutex_);
    return granted_permissions_.find(permission) != granted_permissions_.end();
}

void KSMRouter::EmitEvent(
    const std::string& event,
    const std::string& payload_json,
    const ResponseSender& send_response
) const {
    {
        std::lock_guard lock(state_mutex_);
        if (subscribed_events_.find(event) == subscribed_events_.end()) {
            return;
        }
    }
    Json payload = Json::parse(payload_json, nullptr, false);
    if (payload.is_discarded()) {
        payload = Json::object();
    }
    send_response(Json{
        {"type", "event"},
        {"event", event},
        {"payload", payload}
    }.dump());
}

void KSMRouter::EmitSystemEvent(
    const std::string& event,
    const std::string& payload_json,
    const ResponseSender& send_response
) const {
    Json payload = Json::parse(payload_json, nullptr, false);
    if (payload.is_discarded()) {
        payload = Json::object();
    }
    send_response(Json{
        {"type", "event"},
        {"event", event},
        {"payload", payload}
    }.dump());
}

void KSMRouter::HandleMessage(
    const std::string& message,
    const ResponseSender& send_response
) {
    if (message.size() > 64 * 1024) {
        SendError(
            nullptr,
            "INVALID_ARGS",
            "Request exceeds the 64KB message limit.",
            send_response
        );
        return;
    }

    Json response_id = nullptr;
    try {
        const Json request = Json::parse(message);
        if (!request.is_object()) {
            SendError(nullptr, "INVALID_ARGS", "Request must be an object.", send_response);
            return;
        }

        const Json id = request.contains("id") ? request.at("id") : Json(nullptr);
        response_id = id;
        if (
            request.contains("type") &&
            request.at("type").is_string() &&
            request.at("type") == "cancel"
        ) {
            if (id.is_string()) {
                CancelRequest(id.get<std::string>());
            }
            return;
        }
        if (
            !request.contains("type") ||
            !request.at("type").is_string() ||
            request.at("type") != "request" ||
            !request.contains("id") ||
            !request.contains("cmd") ||
            !request.at("cmd").is_string() ||
            !request.contains("args") ||
            !request.at("args").is_object()
        ) {
            SendError(
                id,
                "INVALID_ARGS",
                "Request fields are invalid.",
                send_response
            );
            return;
        }

        const std::string command = request.at("cmd").get<std::string>();
        const Json& args = request.at("args");

        const auto& known_commands = KSMGeneratedContract::Commands();
        const bool generated_command = Contains(known_commands, command);
        if (
            !generated_command &&
            !plugin_registry_.OwnsCommand(command)
        ) {
            SendError(
                id,
                "UNKNOWN_COMMAND",
                "The command is not registered.",
                send_response
            );
            return;
        }
        if (
            std::find(
                supported_commands_.begin(),
                supported_commands_.end(),
                command
            ) == supported_commands_.end()
        ) {
            SendError(
                id,
                "NOT_SUPPORTED",
                "The command is disabled by application configuration.",
                send_response
            );
            return;
        }
        const bool valid_args = generated_command
            ? KSMGeneratedContract::ValidateArgs(command, args)
            : plugin_registry_.ValidateArgs(command, args);
        if (!valid_args) {
            SendError(
                id,
                "INVALID_ARGS",
                "Arguments do not match the command schema.",
                send_response
            );
            return;
        }

        if (command == "ping") {
            SendSuccess(
                id,
                Json{{"pong", KSM::Phase1::Ping().pong}},
                send_response
            );
            return;
        }

        if (command == "capabilities.query") {
            Json permissions = Json::array();
            {
                std::lock_guard lock(state_mutex_);
                for (const auto& permission : granted_permissions_) {
                    permissions.push_back(permission);
                }
            }
            const auto& metadata = KSMGeneratedContract::PluginMetadata();
            Json plugins = Json::array();
            for (const auto& plugin_id : plugin_ids_) {
                const Json meta = metadata.contains(plugin_id)
                    ? metadata.at(plugin_id)
                    : Json::object();
                plugins.push_back(Json{
                    {"id", plugin_id},
                    {"name", meta.value("name", plugin_id)},
                    {"version", meta.value("version", "1.0.0")},
                    {"enabled", true}
                });
            }
            const std::string app_id = host_services_
                ? host_services_->AppId()
                : "";
            SendSuccess(
                id,
                Json{
                    {"platform", "windows"},
                    {"appId", app_id},
                    {"commands", capability_commands_},
                    {"permissions", permissions},
                    {"plugins", plugins}
                },
                send_response
            );
            return;
        }

        if (command == "app.getInfo") {
            const std::string version = host_services_
                ? host_services_->AppVersion()
                : KASUMI_VERSION_STRING;
            const std::string language = host_services_
                ? host_services_->Language()
                : "en-US";
            SendSuccess(
                id,
                Json{
                    {"name", "Kasumi"},
                    {"version", version},
                    {"platform", "windows"},
                    {"language", language}
                },
                send_response
            );
            return;
        }

        if (command == "device.getInfo") {
            SendError(
                id,
                "NOT_SUPPORTED",
                "device.getInfo is only available on Android.",
                send_response
            );
            return;
        }

        if (command == "permission.request") {
            if (
                !IsString(args, "permission") ||
                args.at("permission").get<std::string>().empty()
            ) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "permission is required.",
                    send_response
                );
                return;
            }
            const std::string permission = args.at("permission").get<std::string>();
            const bool known =
                Contains(KSMGeneratedContract::Permissions(), permission) ||
                plugin_registry_.KnowsPermission(permission);
            if (!known) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "permission is not registered.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"granted", HasPermission(permission)}},
                send_response
            );
            return;
        }

        if (command == "event.subscribe" || command == "event.unsubscribe") {
            if (!IsString(args, "event") || args.at("event").get<std::string>().empty()) {
                SendError(id, "INVALID_ARGS", "event is required.", send_response);
                return;
            }
            const std::string event = args.at("event").get<std::string>();
            const auto& known_events = KSMGeneratedContract::Events();
            if (std::find(known_events.begin(), known_events.end(), event) == known_events.end()) {
                SendError(id, "INVALID_ARGS", "event is not registered.", send_response);
                return;
            }
            if (command == "event.subscribe") {
                {
                    std::lock_guard lock(state_mutex_);
                    subscribed_events_.insert(event);
                }
                SendSuccess(id, Json{{"subscribed", true}}, send_response);
            } else {
                {
                    std::lock_guard lock(state_mutex_);
                    subscribed_events_.erase(event);
                }
                SendSuccess(id, Json{{"unsubscribed", true}}, send_response);
            }
            return;
        }

        if (command == "app.singleInstance.state") {
            const bool primary = host_services_
                ? host_services_->IsPrimaryInstance()
                : true;
            SendSuccess(
                id,
                Json{{"primary", primary}},
                send_response
            );
            return;
        }

        if (command == "app.singleInstance.focus") {
            const bool focused = host_services_
                ? host_services_->FocusMainWindow()
                : false;
            SendSuccess(
                id,
                Json{{"focused", focused}},
                send_response
            );
            return;
        }

        if (command == "tray.show") {
            const bool shown = host_services_
                ? host_services_->TrayShow()
                : false;
            SendSuccess(
                id,
                Json{{"shown", shown}},
                send_response
            );
            return;
        }

        if (command == "tray.hide") {
            const bool hidden = host_services_
                ? host_services_->TrayHide()
                : false;
            SendSuccess(
                id,
                Json{{"hidden", hidden}},
                send_response
            );
            return;
        }

        if (command == "globalShortcut.register") {
            if (!IsString(args, "shortcut")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "shortcut is required.",
                    send_response
                );
                return;
            }
            const bool registered = host_services_
                ? host_services_->RegisterGlobalShortcut(
                    args.at("shortcut").get<std::string>()
                )
                : false;
            SendSuccess(
                id,
                Json{{"registered", registered}},
                send_response
            );
            return;
        }

        if (command == "globalShortcut.unregister") {
            if (!IsString(args, "shortcut")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "shortcut is required.",
                    send_response
                );
                return;
            }
            const bool unregistered = host_services_
                ? host_services_->UnregisterGlobalShortcut(
                    args.at("shortcut").get<std::string>()
                )
                : false;
            SendSuccess(
                id,
                Json{{"unregistered", unregistered}},
                send_response
            );
            return;
        }

        if (command == "updater.check") {
            bool available = false;
            std::string version;
            const bool check = host_services_
                ? host_services_->CheckUpdate(available, version)
                : true;
            if (!check) {
                SendError(
                    id,
                    "IO_ERROR",
                    "The update check failed.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{
                    {"available", available},
                    {"version", version.empty() ? Json(nullptr) : Json(version)}
                },
                send_response
            );
            return;
        }

        if (command == "updater.download") {
            if (!IsString(args, "version") || args.at("version").get<std::string>().empty()) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "version is required.",
                    send_response
                );
                return;
            }
            const bool downloaded = host_services_
                ? host_services_->DownloadUpdate(
                    args.at("version").get<std::string>()
                )
                : false;
            SendSuccess(
                id,
                Json{{"downloaded", downloaded}},
                send_response
            );
            return;
        }

        if (plugin_registry_.OwnsCommand(command)) {
            const auto data = plugin_registry_.Handle(
                command,
                args,
                KSMPluginContext{
                    .file_system = file_system_,
                    .host_services = host_services_,
                    .has_permission = [this](const std::string& permission) {
                        return HasPermission(permission);
                    },
                    .request_id = id.is_string()
                        ? id.get<std::string>()
                        : std::string()
                }
            );
            if (data) {
                SendSuccess(id, *data, send_response);
                return;
            }
        }

        if (command == "fs.readDir" ||
            command == "fs.mkdir" ||
            command == "fs.stat") {
            if (!file_system_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The filesystem is not configured.",
                    send_response
                );
                return;
            }
            const char* permission = command == "fs.mkdir"
                ? "fs.write"
                : "fs.read";
            if (!HasPermission(permission)) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "path") || args.at("path").get<std::string>().empty()) {
                SendError(id, "INVALID_ARGS", "path is required.", send_response);
                return;
            }
            const std::string path = args.at("path").get<std::string>();
            if (command == "fs.mkdir") {
                if (
                    !KSMVirtualPath::IsSafeDirectory(path) ||
                    KSMVirtualPath::IsPicked(path) ||
                    KSMVirtualPath::IsReadOnly(path)
                ) {
                    SendError(
                        id,
                        "PERMISSION_DENIED",
                        "The path is outside the writable sandbox.",
                        send_response
                    );
                    return;
                }
                bool recursive = false;
                if (args.contains("recursive")) {
                    if (!args.at("recursive").is_boolean()) {
                        SendError(
                            id,
                            "INVALID_ARGS",
                            "recursive must be a boolean.",
                            send_response
                        );
                        return;
                    }
                    recursive = args.at("recursive").get<bool>();
                }
                const auto created = file_system_->MakeDirectory(path, recursive);
                if (!created) {
                    SendError(
                        id,
                        "IO_ERROR",
                        "The directory could not be created.",
                        send_response
                    );
                    return;
                }
                SendSuccess(id, Json{{"created", *created}}, send_response);
                return;
            }
            if (command == "fs.readDir") {
                if (
                    !KSMVirtualPath::IsSafeDirectory(path) ||
                    KSMVirtualPath::IsPicked(path)
                ) {
                    SendError(
                        id,
                        "PERMISSION_DENIED",
                        "The path is not a readable directory.",
                        send_response
                    );
                    return;
                }
                const auto entries = file_system_->ReadDir(path);
                if (!entries) {
                    const auto metadata = file_system_->Stat(path);
                    if (metadata && metadata->kind != KSMFileKind::Directory) {
                        SendError(
                            id,
                            "INVALID_ARGS",
                            "The path is not a directory.",
                            send_response
                        );
                    } else {
                        SendError(
                            id,
                            metadata ? "IO_ERROR" : "NOT_FOUND",
                            metadata
                                ? "The directory could not be read."
                                : "The directory was not found.",
                            send_response
                        );
                    }
                    return;
                }
                Json values = Json::array();
                for (const auto& entry : *entries) {
                    values.push_back(DirectoryEntryJson(entry));
                }
                SendSuccess(id, Json{{"entries", values}}, send_response);
                return;
            }
            if (
                !KSMVirtualPath::IsSafeStat(path) ||
                path == "app://picked"
            ) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The path is outside the sandbox.",
                    send_response
                );
                return;
            }
            if (KSMVirtualPath::IsPicked(path)) {
                if (!host_services_) {
                    SendError(
                        id,
                        "NOT_SUPPORTED",
                        "The picked file service is not configured.",
                        send_response
                    );
                    return;
                }
                const auto metadata = host_services_->PickedStat(path);
                if (!metadata) {
                    SendError(
                        id,
                        "NOT_FOUND",
                        "The file was not found.",
                        send_response
                    );
                    return;
                }
                SendSuccess(id, MetadataJson(*metadata), send_response);
                return;
            }
            const auto metadata = file_system_->Stat(path);
            if (!metadata) {
                SendError(
                    id,
                    "NOT_FOUND",
                    "The path was not found.",
                    send_response
                );
                return;
            }
            SendSuccess(id, MetadataJson(*metadata), send_response);
            return;
        }

        if (command == "fs.readText" ||
            command == "fs.writeText" ||
            command == "fs.exists" ||
            command == "fs.remove") {
            if (!file_system_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The filesystem is not configured.",
                    send_response
                );
                return;
            }
            const char* permission = "fs.read";
            if (command == "fs.writeText") {
                permission = "fs.write";
            } else if (command == "fs.remove") {
                permission = "fs.remove";
            }
            if (!HasPermission(permission)) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "path") || args.at("path").get<std::string>().empty()) {
                SendError(id, "INVALID_ARGS", "path is required.", send_response);
                return;
            }
            const std::string path = args.at("path").get<std::string>();
            if (!KSMVirtualPath::IsSafe(path)) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The path is outside the sandbox.",
                    send_response
                );
                return;
            }
            const bool is_picked = KSMVirtualPath::IsPicked(path);
            if (is_picked && !host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The picked file service is not configured.",
                    send_response
                );
                return;
            }
            if (command == "fs.readText") {
                const auto value = is_picked
                    ? host_services_->ReadPickedText(path)
                    : file_system_->ReadText(path);
                if (!value) {
                    const bool exists = is_picked
                        ? host_services_->PickedExists(path)
                        : file_system_->Exists(path);
                    SendError(
                        id,
                        exists ? "IO_ERROR" : "NOT_FOUND",
                        exists
                            ? "The file could not be read."
                            : "The file was not found.",
                        send_response
                    );
                    return;
                }
                if (value->size() > kMaxInlineTextBytes) {
                    if (path.starts_with("app://tmp/read-")) {
                        SendSuccess(id, Json{{"file", path}}, send_response);
                        return;
                    }
                    const auto temporary = file_system_->CreateTemporaryText(*value);
                    if (!temporary) {
                        SendError(
                            id,
                            "IO_ERROR",
                            "The large text result could not be staged.",
                            send_response
                        );
                        return;
                    }
                    SendSuccess(id, Json{{"file", *temporary}}, send_response);
                    return;
                }
                SendSuccess(id, Json(*value), send_response);
                return;
            }
            if (command == "fs.writeText") {
                if (KSMVirtualPath::IsReadOnly(path)) {
                    SendError(
                        id,
                        "PERMISSION_DENIED",
                        "The path is read-only.",
                        send_response
                    );
                    return;
                }
                const bool written = is_picked
                    ? host_services_->WritePickedText(
                        path,
                        args.at("content").get<std::string>()
                    )
                    : file_system_->WriteText(
                        path,
                        args.at("content").get<std::string>()
                    );
                if (!written) {
                    SendError(
                        id,
                        "IO_ERROR",
                        "The file could not be written.",
                        send_response
                    );
                    return;
                }
                SendSuccess(id, Json{{"written", true}}, send_response);
                return;
            }
            if (command == "fs.exists") {
                SendSuccess(
                    id,
                    Json{{"exists", is_picked
                        ? host_services_->PickedExists(path)
                        : file_system_->Exists(path)}},
                    send_response
                );
                return;
            }
            if (KSMVirtualPath::IsReadOnly(path)) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The path is read-only.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"removed", is_picked
                    ? host_services_->RemovePicked(path)
                    : file_system_->Remove(path)}},
                send_response
            );
            return;
        }

        if (command == "dialog.message" || command == "dialog.confirm") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The dialog service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("dialog")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "title") || !IsString(args, "message")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "title and message are required.",
                    send_response
                );
                return;
            }
            const std::string title = args.at("title").get<std::string>();
            const std::string dialog_message =
                args.at("message").get<std::string>();
            const bool confirmed = command == "dialog.message"
                ? host_services_->ShowMessage(title, dialog_message)
                : host_services_->Confirm(title, dialog_message);
            SendSuccess(id, Json{{"confirmed", confirmed}}, send_response);
            return;
        }

        if (command == "dialog.openFile") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The dialog service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("dialog")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            std::vector<std::string> filters;
            if (args.contains("filters")) {
                if (!args.at("filters").is_array()) {
                    SendError(id, "INVALID_ARGS", "filters must be an array.", send_response);
                    return;
                }
                for (const auto& filter : args.at("filters")) {
                    if (!filter.is_string()) {
                        SendError(
                            id,
                            "INVALID_ARGS",
                            "filters must contain strings.",
                            send_response
                        );
                        return;
                    }
                    filters.push_back(filter.get<std::string>());
                }
            }
            const auto file = host_services_->OpenFile(filters);
            Json data = Json::object();
            data["file"] = file ? Json(*file) : Json(nullptr);
            SendSuccess(id, data, send_response);
            return;
        }

        if (command == "dialog.saveFile") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The dialog service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("dialog")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            std::string suggested_name;
            if (args.contains("suggestedName")) {
                if (!args.at("suggestedName").is_string()) {
                    SendError(
                        id,
                        "INVALID_ARGS",
                        "suggestedName must be a string.",
                        send_response
                    );
                    return;
                }
                suggested_name = args.at("suggestedName").get<std::string>();
            }
            const auto file = host_services_->SaveFile(suggested_name);
            Json data = Json::object();
            data["file"] = file ? Json(*file) : Json(nullptr);
            SendSuccess(id, data, send_response);
            return;
        }

        if (command == "clipboard.readText") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The clipboard service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("clipboard.read")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            const auto text = host_services_->ReadClipboardText();
            SendSuccess(id, Json{{"text", text.value_or("")}}, send_response);
            return;
        }

        if (command == "clipboard.writeText") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The clipboard service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("clipboard.write")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "text")) {
                SendError(id, "INVALID_ARGS", "text is required.", send_response);
                return;
            }
            const bool written = host_services_->WriteClipboardText(
                args.at("text").get<std::string>()
            );
            SendSuccess(id, Json{{"written", written}}, send_response);
            return;
        }

        if (command == "notification.show") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The notification service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("notification.show")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "title") || !IsString(args, "body")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "title and body are required.",
                    send_response
                );
                return;
            }
            const bool shown = host_services_->ShowNotification(
                args.at("title").get<std::string>(),
                args.at("body").get<std::string>()
            );
            SendSuccess(id, Json{{"shown", shown}}, send_response);
            return;
        }

        if (command == "window.setTitle") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The window service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("window.control")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "title")) {
                SendError(id, "INVALID_ARGS", "title is required.", send_response);
                return;
            }
            const bool updated = host_services_->SetWindowTitle(
                args.at("title").get<std::string>()
            );
            SendSuccess(id, Json{{"updated", updated}}, send_response);
            return;
        }

        if (command == "window.minimize" || command == "window.maximize") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The window service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("window.control")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            const bool updated = command == "window.minimize"
                ? host_services_->MinimizeWindow()
                : host_services_->MaximizeWindow();
            SendSuccess(id, Json{{"updated", updated}}, send_response);
            return;
        }

        if (command == "window.setSize") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The window service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("window.control")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            const auto& width_value = args.at("width");
            const auto& height_value = args.at("height");
            if (
                (!width_value.is_number_integer() &&
                    !width_value.is_number_unsigned()) ||
                (!height_value.is_number_integer() &&
                    !height_value.is_number_unsigned())
            ) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "width and height must be integers.",
                    send_response
                );
                return;
            }
            const std::int64_t width = width_value.get<std::int64_t>();
            const std::int64_t height = height_value.get<std::int64_t>();
            if (
                width < 320 ||
                height < 240 ||
                width > std::numeric_limits<int>::max() ||
                height > std::numeric_limits<int>::max()
            ) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "width must be at least 320 and height at least 240.",
                    send_response
                );
                return;
            }
            const bool updated = host_services_->SetWindowSize(
                KSMWindowSize{
                    static_cast<int>(width),
                    static_cast<int>(height)
                }
            );
            SendSuccess(id, Json{{"updated", updated}}, send_response);
            return;
        }

        if (command == "shell.openExternal") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The shell service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("shell.open")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (
                !IsString(args, "url") ||
                !KSMUrlPolicy::ParseHttpUrl(
                    args.at("url").get<std::string>()
                )
            ) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "url must be an absolute HTTP or HTTPS URL.",
                    send_response
                );
                return;
            }
            const std::string url = args.at("url").get<std::string>();
            if (!host_services_->IsExternalUrlAllowed(url)) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The URL origin is not allowlisted.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"opened", host_services_->OpenExternal(url)}},
                send_response
            );
            return;
        }

        if (command == "sql.drivers") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The SQL service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("sql")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"drivers", host_services_->SqlDrivers()}},
                send_response
            );
            return;
        }

        if (command == "sql.open") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The SQL service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("sql")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "driver") || !IsString(args, "url")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "driver and url are required.",
                    send_response
                );
                return;
            }
            const auto connection_id = host_services_->SqlOpen(
                args.at("driver").get<std::string>(),
                args.at("url").get<std::string>()
            );
            if (!connection_id) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The requested SQL driver is not available.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"connectionId", *connection_id}},
                send_response
            );
            return;
        }

        if (
            command == "sql.query" ||
            command == "sql.execute" ||
            command == "sql.close"
        ) {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The SQL service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("sql")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (!IsString(args, "connectionId")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "connectionId is required.",
                    send_response
                );
                return;
            }
            const std::string connection_id =
                args.at("connectionId").get<std::string>();
            if (command == "sql.close") {
                SendSuccess(
                    id,
                    Json{{"closed", host_services_->SqlClose(connection_id)}},
                    send_response
                );
                return;
            }
            if (!IsString(args, "statement")) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "statement is required.",
                    send_response
                );
                return;
            }
            std::vector<KSMSqlValue> params;
            if (args.contains("params")) {
                for (const auto& param : args.at("params")) {
                    params.push_back(SqlValueFromJson(param));
                }
            }
            const std::string statement =
                args.at("statement").get<std::string>();
            const std::string request_id = id.is_string()
                ? id.get<std::string>()
                : "";
            if (command == "sql.query") {
                const auto result = host_services_->SqlQuery(
                    connection_id,
                    statement,
                    params,
                    request_id
                );
                if (!result) {
                    SendError(
                        id,
                        "IO_ERROR",
                        "The SQL query could not be completed.",
                        send_response
                    );
                    return;
                }
                Json rows = Json::array();
                for (const auto& row : result->rows) {
                    Json value = Json::object();
                    for (
                        std::size_t index = 0;
                        index < row.size() && index < result->columns.size();
                        ++index
                    ) {
                        value[result->columns[index]] =
                            SqlValueToJson(row[index]);
                    }
                    rows.push_back(std::move(value));
                }
                SendSuccess(
                    id,
                    Json{
                        {"columns", result->columns},
                        {"rows", rows}
                    },
                    send_response
                );
                return;
            }
            const auto result = host_services_->SqlExecute(
                connection_id,
                statement,
                params,
                request_id
            );
            if (!result) {
                SendError(
                    id,
                    "IO_ERROR",
                    "The SQL statement could not be executed.",
                    send_response
                );
                return;
            }
            Json last_insert_id = nullptr;
            if (result->last_insert_id) {
                last_insert_id = *result->last_insert_id;
            }
            SendSuccess(
                id,
                Json{
                    {"rowsAffected", result->rows_affected},
                    {"lastInsertId", last_insert_id}
                },
                send_response
            );
            return;
        }

        if (command == "http.download") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The download service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("http.download")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            if (
                !IsString(args, "url") ||
                !IsString(args, "path")
            ) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "url and path are required.",
                    send_response
                );
                return;
            }
            const std::string url = args.at("url").get<std::string>();
            const std::string path = args.at("path").get<std::string>();
            if (!KSMUrlPolicy::ParseHttpUrl(url, true)) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "url must use HTTPS.",
                    send_response
                );
                return;
            }
            if (
                !KSMVirtualPath::IsSafe(path) ||
                KSMVirtualPath::IsReadOnly(path) ||
                KSMVirtualPath::IsPicked(path)
            ) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "path is outside the writable sandbox.",
                    send_response
                );
                return;
            }
            bool overwrite = false;
            if (args.contains("overwrite")) {
                if (!args.at("overwrite").is_boolean()) {
                    SendError(
                        id,
                        "INVALID_ARGS",
                        "overwrite must be a boolean.",
                        send_response
                    );
                    return;
                }
                overwrite = args.at("overwrite").get<bool>();
            }
            const std::string request_id = id.is_string()
                ? id.get<std::string>()
                : "";
            const auto result = host_services_->Download(
                url,
                path,
                overwrite,
                request_id
            );
            if (!result) {
                SendError(
                    id,
                    "IO_ERROR",
                    "The download could not be completed.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"path", result->path}, {"bytes", result->bytes}},
                send_response
            );
            return;
        }

        if (command == "window.close") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The window service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("window.control")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"closed", host_services_->CloseWindow()}},
                send_response
            );
            return;
        }

        if (command == "app.exit") {
            if (!host_services_) {
                SendError(
                    id,
                    "NOT_SUPPORTED",
                    "The application service is not configured.",
                    send_response
                );
                return;
            }
            if (!HasPermission("window.control")) {
                SendError(
                    id,
                    "PERMISSION_DENIED",
                    "The requested permission was denied.",
                    send_response
                );
                return;
            }
            SendSuccess(
                id,
                Json{{"exited", host_services_->ExitApp()}},
                send_response
            );
            return;
        }

        SendError(
            id,
            "NOT_SUPPORTED",
            "The command is registered but not implemented on this host.",
            send_response
        );
    } catch (const Json::parse_error&) {
        SendError(
            nullptr,
            "INVALID_ARGS",
            "Request is not valid JSON.",
            send_response
        );
    } catch (const Json::exception&) {
        SendError(
            nullptr,
            "INVALID_ARGS",
            "Request fields are invalid.",
            send_response
        );
    } catch (const KSMHostException& error) {
        SendError(
            response_id,
            error.code(),
            error.what(),
            send_response
        );
    } catch (const std::exception&) {
        SendError(
            nullptr,
            "INTERNAL_ERROR",
            "The command failed unexpectedly.",
            send_response
        );
    }
}
