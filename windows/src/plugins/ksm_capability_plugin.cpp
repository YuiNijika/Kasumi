#include "ksm_capability_plugin.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../generated/contract.h"
#include "../platform/ksm_host_services.h"
#include "../platform/ksm_url_policy.h"
#include "../platform/ksm_virtual_path.h"

namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxInlineBytes = 60 * 1024;

const std::vector<std::string>& CommandNames() {
    static const std::vector<std::string> commands = {
        "http.request",
        "http.transfer.start",
        "http.transfer.cancel",
        "http.cookies.get",
        "http.cookies.set",
        "http.cookies.clear",
        "dialog.openFolder",
        "dialog.openFiles",
        "fs.walk",
        "fs.scope.release",
        "media.probe",
        "media.ffmpeg.detect",
        "media.ffmpeg.validate",
        "media.ffmpeg.setPath",
        "media.ffmpeg.pickExecutable",
        "audio.devices.list",
        "audio.devices.set",
        "audio.output.getMode",
        "audio.output.setExclusive",
        "audio.eq.set",
        "audio.speed.set",
        "audio.probe",
        "audio.load",
        "audio.play",
        "audio.pause",
        "audio.stop",
        "audio.seek",
        "audio.volume.set",
        "audio.state.get",
        "mediaSession.update",
        "mediaSession.clear",
        "window.create",
        "window.show",
        "window.hide",
        "window.setPosition",
        "window.setAlwaysOnTop",
        "window.setTransparent",
        "window.getState",
        "event.emit",
        "tray.configure",
        "updater.install",
        "app.openDevtools",
        "app.deepLink.getPending"
    };
    return commands;
}

const std::vector<std::string>& EventNames() {
    static const std::vector<std::string> events = {
        "http.transfer.progress",
        "http.transfer.completed",
        "http.transfer.failed",
        "audio.tick",
        "audio.ended",
        "audio.stateChanged",
        "audio.error",
        "mediaSession.command",
        "window.visibilityChanged",
        "window.stateChanged",
        "tray.action",
        "tray.click",
        "globalShortcut.triggered",
        "updater.progress",
        "updater.status",
        "app.deepLink.received",
        "app.secondInstance.received"
    };
    return events;
}

std::string RequiredString(const Json& args, const char* field) {
    if (!args.contains(field) || !args.at(field).is_string()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " must be a string."
        );
    }
    const std::string value = args.at(field).get<std::string>();
    if (value.empty()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " is required."
        );
    }
    return value;
}

std::string OptionalString(const Json& args, const char* field) {
    if (!args.contains(field) || args.at(field).is_null()) {
        return {};
    }
    if (!args.at(field).is_string()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " must be a string."
        );
    }
    return args.at(field).get<std::string>();
}

std::vector<std::string> StringArray(const Json& args, const char* field) {
    std::vector<std::string> values;
    if (!args.contains(field) || args.at(field).is_null()) {
        return values;
    }
    if (!args.at(field).is_array()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " must be an array."
        );
    }
    for (const auto& value : args.at(field)) {
        if (!value.is_string()) {
            throw KSMHostException(
                "INVALID_ARGS",
                std::string(field) + " must contain strings."
            );
        }
        values.push_back(value.get<std::string>());
    }
    return values;
}

std::vector<std::string> FolderFilters(const Json& args) {
    if (!args.contains("filters") || args.at("filters").is_null()) {
        return {};
    }
    const auto& raw = args.at("filters");
    if (raw.is_array()) {
        std::vector<std::string> values;
        for (const auto& value : raw) {
            if (!value.is_string()) {
                throw KSMHostException(
                    "INVALID_ARGS",
                    "filters must contain strings."
                );
            }
            values.push_back(value.get<std::string>());
        }
        return values;
    }
    if (!raw.is_object()) {
        throw KSMHostException(
            "INVALID_ARGS",
            "filters must be an object."
        );
    }
    if (raw.contains("mimeTypes")) {
        const Json wrapped{{"filters", raw.at("mimeTypes")}};
        return StringArray(wrapped, "filters");
    }
    if (raw.contains("extensions") && !raw.at("extensions").empty()) {
        return {"*/*"};
    }
    return {};
}

std::vector<KSMHttpHeader> Headers(const Json& args) {
    std::vector<KSMHttpHeader> headers;
    if (!args.contains("headers") || args.at("headers").is_null()) {
        return headers;
    }
    const auto& raw = args.at("headers");
    if (!raw.is_object()) {
        throw KSMHostException(
            "INVALID_ARGS",
            "headers must be an object."
        );
    }
    static const std::set<std::string> blocked = {
        "cookie",
        "host",
        "proxy-authorization",
        "set-cookie",
        "user-agent",
        "referer"
    };
    for (auto iterator = raw.begin(); iterator != raw.end(); ++iterator) {
        if (!iterator.value().is_string()) {
            throw KSMHostException(
                "INVALID_ARGS",
                "header values must be strings."
            );
        }
        std::string name = iterator.key();
        std::transform(
            name.begin(),
            name.end(),
            name.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            }
        );
        if (blocked.contains(name)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The header is controlled by the host."
            );
        }
        headers.push_back(KSMHttpHeader{
            iterator.key(),
            iterator.value().get<std::string>()
        });
    }
    return headers;
}

std::string HttpMethod(const Json& args) {
    const std::string method = OptionalString(args, "method");
    const std::string value = method.empty() ? "GET" : method;
    if (value != "GET" && value != "POST") {
        throw KSMHostException(
            "INVALID_ARGS",
            "method must be GET or POST."
        );
    }
    return value;
}

std::string HttpBody(const Json& args) {
    const std::string body = OptionalString(args, "body");
    if (body.size() > kMaxInlineBytes) {
        throw KSMHostException(
            "RESOURCE_LIMIT",
            "The request body exceeds the inline message limit."
        );
    }
    return body;
}

std::string HttpUrl(
    const Json& args,
    const std::shared_ptr<KSMHostServices>& host
) {
    const std::string url = RequiredString(args, "url");
    if (!KSMUrlPolicy::ParseHttpUrl(url, true)) {
        throw KSMHostException("INVALID_ARGS", "url must use HTTPS.");
    }
    if (!host || !host->IsHttpRequestUrlAllowed(url)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The URL origin is not allowlisted."
        );
    }
    return url;
}

std::string SafePath(const Json& args, bool writable) {
    const std::string path = RequiredString(args, "path");
    if (!KSMVirtualPath::IsSafe(path)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the sandbox."
        );
    }
    if (
        writable &&
        (KSMVirtualPath::IsReadOnly(path) || KSMVirtualPath::IsPicked(path))
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside the writable sandbox."
        );
    }
    return path;
}

std::string ScopePath(const Json& args) {
    const std::string path = RequiredString(args, "path");
    const std::string prefix = "app://scope/";
    if (!path.starts_with(prefix)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside an active file scope."
        );
    }
    const std::string relative = path.substr(prefix.size());
    if (
        relative.empty() ||
        relative.front() == '/' ||
        relative.find("..") != std::string::npos ||
        relative.find('\0') != std::string::npos ||
        relative.find('\\') != std::string::npos ||
        relative.find(':') != std::string::npos ||
        relative.find("//") != std::string::npos
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside an active file scope."
        );
    }
    return path;
}

KSMHttpRequestSpec RequestSpec(const Json& args) {
    KSMHttpRequestSpec request;
    request.url = RequiredString(args, "url");
    request.method = HttpMethod(args);
    request.headers = Headers(args);
    request.body = HttpBody(args);
    const std::string jar_id = OptionalString(args, "jarId");
    if (!jar_id.empty()) {
        request.jar_id = jar_id;
    }
    const std::string user_agent = OptionalString(args, "userAgent");
    if (!user_agent.empty()) {
        request.user_agent = user_agent;
    }
    const std::string referer = OptionalString(args, "referer");
    if (!referer.empty()) {
        request.referer = referer;
    }
    return request;
}

KSMHttpTransferSpec TransferSpec(const Json& args) {
    KSMHttpTransferSpec request;
    request.url = RequiredString(args, "url");
    request.method = HttpMethod(args);
    request.path = SafePath(args, true);
    request.headers = Headers(args);
    request.body = HttpBody(args);
    request.overwrite = args.value("overwrite", false);
    const std::string jar_id = OptionalString(args, "jarId");
    if (!jar_id.empty()) {
        request.jar_id = jar_id;
    }
    const std::string user_agent = OptionalString(args, "userAgent");
    if (!user_agent.empty()) {
        request.user_agent = user_agent;
    }
    const std::string referer = OptionalString(args, "referer");
    if (!referer.empty()) {
        request.referer = referer;
    }
    return request;
}

std::vector<KSMCookie> Cookies(const Json& args) {
    std::vector<KSMCookie> cookies;
    if (!args.contains("cookies") || !args.at("cookies").is_array()) {
        throw KSMHostException(
            "INVALID_ARGS",
            "cookies must be an array."
        );
    }
    for (const auto& value : args.at("cookies")) {
        if (!value.is_object()) {
            throw KSMHostException(
                "INVALID_ARGS",
                "cookies must contain objects."
            );
        }
        KSMCookie cookie;
        cookie.name = RequiredString(value, "name");
        cookie.value = OptionalString(value, "value");
        cookie.domain = OptionalString(value, "domain");
        cookie.path = OptionalString(value, "path");
        cookie.secure = value.value("secure", true);
        cookie.http_only = value.value("httpOnly", false);
        cookies.push_back(std::move(cookie));
    }
    return cookies;
}

Json ResponseJson(const KSMHttpResponse& response) {
    Json headers = Json::object();
    for (const auto& header : response.headers) {
        headers[header.name] = header.value;
    }
    return Json{
        {"status", response.status},
        {"headers", headers},
        {"body", response.body},
        {"file", response.file ? Json(*response.file) : Json(nullptr)},
        {"bytes", response.bytes}
    };
}

Json ScopeJson(const KSMFileScope& scope) {
    return Json{
        {"id", scope.id},
        {"path", scope.path},
        {"files", scope.files}
    };
}

Json WalkJson(const std::vector<KSMWalkEntry>& entries) {
    Json values = Json::array();
    for (const auto& entry : entries) {
        values.push_back(Json{
            {"path", entry.path},
            {"name", entry.name},
            {"directory", entry.directory},
            {"size", entry.size},
            {"modifiedAt", entry.modified_at_ms
                ? Json(*entry.modified_at_ms)
                : Json(nullptr)}
        });
    }
    return Json{{"entries", values}};
}

bool IsAudioCommand(const std::string& command) {
    return command.starts_with("audio.");
}

bool IsMediaCommand(const std::string& command) {
    return command == "media.probe" || command.starts_with("media.ffmpeg.");
}

bool IsWindowCommand(const std::string& command) {
    return command.starts_with("window.");
}

void RequireHost(const KSMPluginContext& context) {
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The host service is not configured."
        );
    }
}

std::string CommandPermission(const std::string& command) {
    if (command.starts_with("http.")) {
        if (command.starts_with("http.transfer.")) {
            return "http.transfer";
        }
        if (command.starts_with("http.cookies.")) {
            return "http.cookies";
        }
        return "http.request";
    }
    if (command.starts_with("dialog.")) {
        return "dialog";
    }
    if (command.starts_with("fs.")) {
        return "fs.read";
    }
    if (
        command == "media.probe" ||
        command.starts_with("media.ffmpeg.") ||
        command == "audio.probe" ||
        command == "audio.state.get" ||
        command == "audio.devices.list" ||
        command == "audio.output.getMode"
    ) {
        return "media.read";
    }
    if (command.starts_with("audio.")) {
        return "media.control";
    }
    if (command.starts_with("mediaSession.")) {
        return "mediaSession";
    }
    if (command.starts_with("window.") || command == "event.emit") {
        return "window.manage";
    }
    if (command.starts_with("tray.")) {
        return "tray.configure";
    }
    if (command.starts_with("updater.")) {
        return "updater.install";
    }
    if (command == "app.openDevtools") {
        return "app.devtools";
    }
    if (command == "app.deepLink.getPending") {
        return "app.deepLink";
    }
    return "window.control";
}

void RequireCommandPermission(
    const std::string& command,
    const KSMPluginContext& context
) {
    const std::string permission = CommandPermission(command);
    if (!context.has_permission || !context.has_permission(permission)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The requested permission was denied."
        );
    }
}

Json FailClosed(const std::string& command) {
    throw KSMHostException(
        "NOT_SUPPORTED",
        command + " is not implemented on this host."
    );
}

void RequireResult(const std::string& command, bool result) {
    if (!result) {
        FailClosed(command);
    }
}

Json HttpCommand(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequireHost(context);
    if (command == "http.request") {
        auto request = RequestSpec(args);
        request.url = HttpUrl(args, context.host_services);
        const auto response = context.host_services->HttpRequest(
            request,
            context.request_id
        );
        if (!response) {
            return FailClosed(command);
        }
        return ResponseJson(*response);
    }
    if (command == "http.transfer.start") {
        auto request = TransferSpec(args);
        request.url = HttpUrl(args, context.host_services);
        const auto started = context.host_services->StartHttpTransfer(
            request,
            context.request_id
        );
        if (!started) {
            return FailClosed(command);
        }
        return Json{{"transferId", started->transfer_id}};
    }
    if (command == "http.transfer.cancel") {
        const std::string transfer_id = RequiredString(args, "transferId");
        RequireResult(
            command,
            context.host_services->CancelHttpTransfer(transfer_id)
        );
        return Json{{"cancelled", true}};
    }
    const std::string jar_id = RequiredString(args, "jarId");
    const std::string url = HttpUrl(args, context.host_services);
    if (command == "http.cookies.get") {
        const auto cookies = context.host_services->GetCookies(jar_id, url);
        if (!cookies) {
            return FailClosed(command);
        }
        if (cookies->empty()) {
            return Json{{"cookies", Json::array()}};
        }
        Json values = Json::array();
        for (const auto& cookie : *cookies) {
            values.push_back(Json{
                {"name", cookie.name},
                {"value", cookie.value},
                {"domain", cookie.domain},
                {"path", cookie.path},
                {"secure", cookie.secure},
                {"httpOnly", cookie.http_only}
            });
        }
        return Json{{"cookies", values}};
    }
    if (command == "http.cookies.set") {
        RequireResult(
            command,
            context.host_services->SetCookies(jar_id, url, Cookies(args))
        );
        return Json{{"updated", true}};
    }
    RequireResult(
        command,
        context.host_services->ClearCookies(jar_id, url)
    );
    return Json{{"cleared", true}};
}

Json FileCommand(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequireHost(context);
    if (command == "dialog.openFolder" || command == "dialog.openFiles") {
        const auto filters = command == "dialog.openFolder"
            ? FolderFilters(args)
            : StringArray(args, "filters");
        const auto scope = command == "dialog.openFolder"
            ? context.host_services->OpenFolder(filters)
            : context.host_services->OpenFiles(filters);
        if (!scope) {
            return FailClosed(command);
        }
        return ScopeJson(*scope);
    }
    if (command == "fs.walk") {
        const std::string path = ScopePath(args);
        const auto entries = context.host_services->Walk(
            path,
            context.request_id
        );
        if (!entries) {
            return FailClosed(command);
        }
        return WalkJson(*entries);
    }
    if (command == "fs.scope.release") {
        RequireResult(
            command,
            context.host_services->ReleaseScope(
                RequiredString(args, "scopeId")
            )
        );
        return Json{{"released", true}};
    }
    if (command == "media.probe") {
        const auto probe = context.host_services->ProbeMedia(ScopePath(args));
        if (!probe) {
            return FailClosed(command);
        }
        return Json{
            {"title", probe->title ? Json(*probe->title) : Json(nullptr)},
            {"artist", probe->artist ? Json(*probe->artist) : Json(nullptr)},
            {"album", probe->album ? Json(*probe->album) : Json(nullptr)},
            {"durationMs", probe->duration_ms
                ? Json(*probe->duration_ms)
                : Json(nullptr)},
            {"cover", probe->cover ? Json(*probe->cover) : Json(nullptr)},
            {"lyric", probe->lyrics ? Json(*probe->lyrics) : Json(nullptr)},
            {"format", probe->format ? Json(*probe->format) : Json(nullptr)}
        };
    }
    std::string name = "ffmpeg";
    std::string path;
    if (command == "media.ffmpeg.detect") {
        name = OptionalString(args, "name");
        if (name.empty()) {
            name = "ffmpeg";
        }
    } else if (command == "media.ffmpeg.pickExecutable") {
        name = OptionalString(args, "name");
        if (name.empty()) {
            name = "ffmpeg";
        }
    } else if (command == "media.ffmpeg.validate") {
        path = SafePath(args, false);
    } else {
        name = OptionalString(args, "name");
        if (name.empty()) {
            name = "ffmpeg";
        }
        path = SafePath(args, false);
    }
    std::optional<KSMExternalTool> tool;
    if (command == "media.ffmpeg.detect") {
        tool = context.host_services->DetectExternalTool(name);
    } else if (command == "media.ffmpeg.validate") {
        tool = context.host_services->ValidateExternalTool(path);
    } else if (command == "media.ffmpeg.setPath") {
        RequireResult(
            command,
            context.host_services->SetExternalToolPath(name, path)
        );
        return Json{{"updated", true}};
    } else {
        const auto picked = context.host_services->PickExternalTool(name);
        if (!picked) {
            return FailClosed(command);
        }
        return Json{{"path", *picked}};
    }
    if (!tool) {
        return FailClosed(command);
    }
    if (command == "media.ffmpeg.detect") {
        return Json{
            {"available", tool->valid},
            {"path", tool->valid ? Json(tool->path) : Json(nullptr)},
            {"version", tool->version.empty()
                ? Json(nullptr)
                : Json(tool->version)},
            {"source", nullptr}
        };
    }
    return Json{
        {"valid", tool->valid},
        {"version", tool->version.empty()
            ? Json(nullptr)
            : Json(tool->version)}
    };
}

Json AudioCommand(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequireHost(context);
    if (command == "audio.devices.list") {
        const auto devices = context.host_services->AudioDevices();
        if (!devices) {
            return FailClosed(command);
        }
        Json values = Json::array();
        for (const auto& device : *devices) {
            values.push_back(Json{
                {"id", device.id},
                {"name", device.name},
                {"isDefault", device.default_device}
            });
        }
        return Json{{"devices", values}};
    }
    if (command == "audio.output.getMode") {
        const auto state = context.host_services->GetAudioState();
        if (!state) {
            return FailClosed(command);
        }
        return Json{{"mode", "shared"}};
    }
    if (command == "audio.state.get") {
        const auto state = context.host_services->GetAudioState();
        if (!state) {
            return FailClosed(command);
        }
        return Json{
            {"playback", state->playback},
            {"positionMs", state->position_ms},
            {"durationMs", state->duration_ms
                ? Json(*state->duration_ms)
                : Json(nullptr)},
            {"volume", state->volume},
            {"muted", state->muted},
            {"speed", state->speed},
            {"outputMode", state->output_mode},
            {"deviceId", state->device_id
                ? Json(*state->device_id)
                : Json(nullptr)},
            {"source", state->source
                ? Json(*state->source)
                : Json(nullptr)}
        };
    }
    if (command == "audio.probe") {
        const auto probe = context.host_services->ProbeAudio(
            ScopePath(args)
        );
        if (!probe) {
            return FailClosed(command);
        }
        return Json{
            {"durationMs", probe->duration_ms
                ? Json(*probe->duration_ms)
                : Json(nullptr)},
            {"sampleRate", probe->sample_rate
                ? Json(*probe->sample_rate)
                : Json(nullptr)},
            {"channels", probe->channels
                ? Json(*probe->channels)
                : Json(nullptr)},
            {"codec", probe->codec
                ? Json(*probe->codec)
                : Json(nullptr)}
        };
    }
    if (command == "audio.devices.set") {
        RequireResult(
            command,
            context.host_services->SetAudioDevice(
                RequiredString(args, "deviceId")
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "audio.output.setExclusive") {
        RequireResult(
            command,
            context.host_services->SetExclusiveAudioOutput(
                args.at("exclusive").get<bool>()
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "audio.eq.set") {
        std::vector<double> gains;
        for (const auto& gain : args.at("gains")) {
            gains.push_back(gain.get<double>());
        }
        RequireResult(
            command,
            context.host_services->SetAudioEqualizer(gains)
        );
        return Json{{"updated", true}};
    }
    if (command == "audio.speed.set") {
        RequireResult(
            command,
            context.host_services->SetAudioSpeed(
                args.at("speed").get<double>()
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "audio.volume.set") {
        RequireResult(
            command,
            context.host_services->SetAudioVolume(
                args.at("volume").get<double>()
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "audio.load") {
        RequireResult(
            command,
            context.host_services->LoadAudio(ScopePath(args))
        );
        return Json{{"loaded", true}};
    }
    if (command == "audio.play" || command == "audio.pause") {
        RequireResult(
            command,
            context.host_services->SetAudioPlayback(command == "audio.play")
        );
        return Json{{"updated", true}};
    }
    if (command == "audio.stop") {
        RequireResult(
            command,
            context.host_services->SetAudioPlayback(false) &&
                context.host_services->SeekAudio(0)
        );
        return Json{{"updated", true}};
    }
    RequireResult(
        command,
        context.host_services->SeekAudio(
            args.at("positionMs").get<std::int64_t>()
        )
    );
    return Json{{"updated", true}};
}

Json WindowStateJson(const KSMWindowState& state) {
    return Json{
        {"visible", state.visible},
        {"alwaysOnTop", state.always_on_top},
        {"transparent", state.transparent},
        {"x", state.x},
        {"y", state.y},
        {"width", state.width},
        {"height", state.height}
    };
}

KSMWindowState WindowState(const Json& args) {
    KSMWindowState state;
    state.visible = args.value("visible", true);
    state.always_on_top = args.value("alwaysOnTop", false);
    state.transparent = args.value("transparent", false);
    state.x = args.value("x", 0);
    state.y = args.value("y", 0);
    state.width = args.value("width", 0);
    state.height = args.value("height", 0);
    return state;
}

Json WindowCommand(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequireHost(context);
    if (command == "window.create") {
        const auto window_id = context.host_services->CreateWindow(
            OptionalString(args, "route"),
            WindowState(args)
        );
        if (!window_id) {
            return FailClosed(command);
        }
        return Json{{"windowId", *window_id}};
    }
    const std::string window_id = RequiredString(args, "windowId");
    if (command == "window.show" || command == "window.hide") {
        RequireResult(
            command,
            context.host_services->ShowWindowById(
                window_id,
                command == "window.show"
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "window.setPosition") {
        RequireResult(
            command,
            context.host_services->SetWindowPosition(
                window_id,
                args.at("x").get<int>(),
                args.at("y").get<int>()
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "window.setAlwaysOnTop") {
        RequireResult(
            command,
            context.host_services->SetWindowAlwaysOnTop(
                window_id,
                args.at("enabled").get<bool>()
            )
        );
        return Json{{"updated", true}};
    }
    if (command == "window.setTransparent") {
        RequireResult(
            command,
            context.host_services->SetWindowTransparent(
                window_id,
                args.at("enabled").get<bool>()
            )
        );
        return Json{{"updated", true}};
    }
    const auto state = context.host_services->GetWindowState(window_id);
    if (!state) {
        return FailClosed(command);
    }
    return WindowStateJson(*state);
}

Json SystemCommand(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequireHost(context);
    if (command == "mediaSession.update") {
        KSMMediaSessionMetadata metadata;
        metadata.title = OptionalString(args, "title");
        metadata.artist = OptionalString(args, "artist");
        metadata.album = OptionalString(args, "album");
        const std::string artwork = OptionalString(args, "artwork");
        if (!artwork.empty()) {
            metadata.artwork = artwork;
        }
        if (
            args.contains("durationMs") &&
            !args.at("durationMs").is_null()
        ) {
            metadata.duration_ms =
                args.at("durationMs").get<std::int64_t>();
        }
        if (
            args.contains("positionMs") &&
            !args.at("positionMs").is_null()
        ) {
            metadata.position_ms =
                args.at("positionMs").get<std::int64_t>();
        }
        metadata.state = OptionalString(args, "state");
        RequireResult(
            command,
            context.host_services->UpdateMediaSession(metadata)
        );
        return Json{{"updated", true}};
    }
    if (command == "mediaSession.clear") {
        RequireResult(
            command,
            context.host_services->ClearMediaSession()
        );
        return Json{{"cleared", true}};
    }
    if (command == "event.emit") {
        const std::string event = RequiredString(args, "event");
        const auto& generated_events = KSMGeneratedContract::Events();
        const auto& capability_events = EventNames();
        if (
            std::find(
                generated_events.begin(),
                generated_events.end(),
                event
            ) == generated_events.end() &&
            std::find(
                capability_events.begin(),
                capability_events.end(),
                event
            ) == capability_events.end()
        ) {
            throw KSMHostException(
                "INVALID_ARGS",
                "event is not registered."
            );
        }
        const std::string window_id = OptionalString(args, "windowId");
        const Json payload = args.contains("payload") && args.at("payload").is_object()
            ? args.at("payload")
            : Json::object();
        if (payload.dump().size() > kMaxInlineBytes) {
            throw KSMHostException(
                "RESOURCE_LIMIT",
                "The event payload exceeds the inline message limit."
            );
        }
        if (
            window_id.empty() || window_id == context.window_id
        ) {
            if (!context.emit_event(event, payload)) {
                return FailClosed(command);
            }
            return Json{{"emitted", true}};
        }
        RequireResult(
            command,
            context.host_services->EmitWindowEvent(
                window_id,
                event,
                payload.dump()
            )
        );
        return Json{{"emitted", true}};
    }
    if (command == "tray.configure") {
        KSMTrayConfig config;
        const std::string icon = OptionalString(args, "icon");
        if (!icon.empty()) {
            config.icon = icon;
        }
        const std::string tooltip = OptionalString(args, "tooltip");
        if (!tooltip.empty()) {
            config.tooltip = tooltip;
        }
        if (args.contains("items") && args.at("items").is_array()) {
            for (const auto& item : args.at("items")) {
                config.items.push_back(KSMTrayMenuItem{
                    RequiredString(item, "id"),
                    OptionalString(item, "title"),
                    item.value("enabled", true)
                });
            }
        }
        RequireResult(
            command,
            context.host_services->ConfigureTray(config)
        );
        return Json{{"configured", true}};
    }
    if (command == "updater.install") {
        RequireResult(
            command,
            context.host_services->InstallUpdate(
                SafePath(args, false),
                OptionalString(args, "hash"),
                OptionalString(args, "signature"),
                context.request_id
            )
        );
        return Json{{"installed", true}};
    }
    if (command == "app.openDevtools") {
        RequireResult(
            command,
            context.host_services->OpenDevtools(
                args.value("enabled", true)
            )
        );
        return Json{{"opened", true}};
    }
    const auto pending = context.host_services->TakePendingDeepLink();
    return Json{{
        "url",
        pending ? Json(*pending) : Json(nullptr)
    }};
}

}

KSMCapabilityPlugin::KSMCapabilityPlugin(
    std::vector<std::string> commands
) : commands_(std::move(commands)) {}

const std::string& KSMCapabilityPlugin::Id() const {
    static const std::string id = "capabilities";
    return id;
}

const std::vector<std::string>& KSMCapabilityPlugin::Commands() const {
    return commands_;
}

const std::vector<std::string>& KSMCapabilityPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {
        "http.request",
        "http.transfer",
        "http.cookies",
        "dialog",
        "fs.read",
        "media.read",
        "media.control",
        "mediaSession",
        "window.manage",
        "tray.configure",
        "updater.install",
        "app.devtools",
        "app.deepLink"
    };
    return permissions;
}

bool KSMCapabilityPlugin::ValidateArgs(
    const std::string& command,
    const Json& args
) const {
    if (!args.is_object()) {
        return false;
    }
    if (command == "http.transfer.cancel") {
        return args.contains("transferId") &&
            args.at("transferId").is_string();
    }
    if (command == "http.request" ||
        command == "http.transfer.start") {
        return args.contains("url") && args.at("url").is_string();
    }
    if (command.starts_with("http.cookies.")) {
        return args.contains("jarId") &&
            args.at("jarId").is_string() &&
            args.contains("url") &&
            args.at("url").is_string();
    }
    if (
        command == "media.probe" ||
        command == "fs.walk" ||
        command == "audio.load" ||
        command == "audio.probe"
    ) {
        return args.contains("path") && args.at("path").is_string();
    }
    if (command == "fs.scope.release") {
        return args.contains("scopeId");
    }
    if (command.starts_with("window.")) {
        return command == "window.create" || args.contains("windowId");
    }
    if (command == "event.emit") {
        return args.contains("event");
    }
    if (command == "audio.eq.set") {
        return args.contains("gains");
    }
    if (command == "audio.speed.set") {
        return args.contains("speed");
    }
    if (command == "audio.volume.set") {
        return args.contains("volume");
    }
    if (command == "audio.seek") {
        return args.contains("positionMs");
    }
    if (command == "audio.devices.set") {
        return args.contains("deviceId");
    }
    if (command == "audio.output.setExclusive") {
        return args.contains("exclusive");
    }
    if (command == "window.setAlwaysOnTop" ||
        command == "window.setTransparent") {
        return args.contains("enabled");
    }
    if (command == "app.openDevtools") {
        return true;
    }
    return true;
}

Json KSMCapabilityPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequireCommandPermission(command, context);
    if (
        (command == "http.request" ||
            command == "http.transfer.start" ||
            command.starts_with("http.cookies.")) &&
        (!args.contains("url") || !args.at("url").is_string())
    ) {
        throw KSMHostException("INVALID_ARGS", "url is required.");
    }
    if (
        (command == "window.setAlwaysOnTop" ||
            command == "window.setTransparent") &&
        (!args.contains("enabled") || !args.at("enabled").is_boolean())
    ) {
        throw KSMHostException("INVALID_ARGS", "enabled is required.");
    }
    if (
        command == "audio.output.setExclusive" &&
        (!args.contains("exclusive") || !args.at("exclusive").is_boolean())
    ) {
        throw KSMHostException("INVALID_ARGS", "exclusive is required.");
    }
    if (
        (command == "audio.speed.set" ||
            command == "audio.volume.set") &&
        (!args.contains(command == "audio.speed.set" ? "speed" : "volume") ||
            !args.at(command == "audio.speed.set" ? "speed" : "volume")
                .is_number())
    ) {
        throw KSMHostException("INVALID_ARGS", "A number is required.");
    }
    if (
        command == "audio.seek" &&
        (!args.contains("positionMs") ||
            !args.at("positionMs").is_number_integer())
    ) {
        throw KSMHostException("INVALID_ARGS", "positionMs is required.");
    }
    if (
        command == "window.setPosition" &&
        (!args.contains("x") || !args.at("x").is_number_integer() ||
            !args.contains("y") || !args.at("y").is_number_integer())
    ) {
        throw KSMHostException("INVALID_ARGS", "x and y are required.");
    }
    if (
        command == "audio.eq.set" &&
        (!args.contains("gains") || !args.at("gains").is_array())
    ) {
        throw KSMHostException("INVALID_ARGS", "gains is required.");
    }
    if (command.starts_with("http.")) {
        return HttpCommand(command, args, context);
    }
    if (
        command.starts_with("dialog.") ||
        command.starts_with("fs.") ||
        IsMediaCommand(command)
    ) {
        return FileCommand(command, args, context);
    }
    if (IsAudioCommand(command)) {
        return AudioCommand(command, args, context);
    }
    if (IsWindowCommand(command)) {
        return WindowCommand(command, args, context);
    }
    return SystemCommand(command, args, context);
}

std::vector<std::string> KSMCapabilityCommands() {
    return CommandNames();
}

std::vector<std::string> KSMCapabilityEvents() {
    return EventNames();
}

bool KSMIsCapabilityCommand(const std::string& command) {
    const auto& commands = CommandNames();
    return std::find(commands.begin(), commands.end(), command) !=
        commands.end();
}
