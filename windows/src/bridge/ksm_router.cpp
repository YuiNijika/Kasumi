#include "ksm_router.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "../commands/ksm_phase1.h"
#include "../generated/contract.h"
#include "../platform/ksm_host_services.h"
#include "../platform/ksm_url_policy.h"
#include "../platform/ksm_virtual_path.h"
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

}

KSMRouter::KSMRouter(
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services
)
    : file_system_(std::move(file_system)),
      host_services_(std::move(host_services)) {
    const auto& permissions = KSMGeneratedContract::Permissions();
    granted_permissions_.insert(permissions.begin(), permissions.end());
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
        if (std::find(known_commands.begin(), known_commands.end(), command) == known_commands.end()) {
            SendError(
                id,
                "UNKNOWN_COMMAND",
                "The command is not registered.",
                send_response
            );
            return;
        }
        if (!KSMGeneratedContract::ValidateArgs(command, args)) {
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
            SendSuccess(
                id,
                Json{
                    {"platform", "windows"},
                    {"commands", KSMCommandRegistry::SupportedCommands()},
                    {"permissions", permissions}
                },
                send_response
            );
            return;
        }

        if (command == "app.getInfo") {
            const std::string version = host_services_
                ? host_services_->AppVersion()
                : "0.1.0";
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
            const auto& permissions = KSMGeneratedContract::Permissions();
            const bool known = std::find(
                permissions.begin(),
                permissions.end(),
                permission
            ) != permissions.end();
            if (!known) {
                SendError(
                    id,
                    "INVALID_ARGS",
                    "permission is not registered.",
                    send_response
                );
                return;
            }
            {
                std::lock_guard lock(state_mutex_);
                granted_permissions_.insert(permission);
            }
            SendSuccess(id, Json{{"granted", true}}, send_response);
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
