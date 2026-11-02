#include "ksm_phase1.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "../bridge/ksm_command_registry.h"
#include "../bridge/ksm_router.h"
#include "../generated/contract.h"
#include "../platform/ksm_host_services.h"
#include "../platform/ksm_virtual_path.h"
#include "ksm_filesystem.h"

namespace {

using Json = nlohmann::json;

struct RuntimeState {
    std::mutex mutex;
    std::shared_ptr<KSMFileSystem> file_system;
    std::shared_ptr<KSMHostServices> host_services;
    std::vector<std::string> allowed_commands;
    std::unordered_map<std::string, std::shared_ptr<KSMRouter>> routers;
    std::unordered_map<
        std::uint64_t,
        std::pair<std::string, KSM::EventHandler>
    > listeners;
    std::uint64_t next_subscription = 1;
    std::atomic<std::uint64_t> next_request{1};
    bool initialized = false;
};

RuntimeState& State() {
    static RuntimeState state;
    return state;
}

std::shared_ptr<KSMRouter> RequireRouter(std::string_view window_id) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) {
        throw std::logic_error("KSM must be initialized before use.");
    }
    const std::string key(window_id);
    const auto existing = state.routers.find(key);
    if (existing != state.routers.end()) {
        return existing->second;
    }
    auto router = std::make_shared<KSMRouter>(
        state.file_system,
        state.host_services,
        state.allowed_commands
    );
    state.routers.emplace(key, router);
    return router;
}

bool IsKnownEvent(const std::string& event) {
    const auto& events = KSMGeneratedContract::Events();
    return std::find(events.begin(), events.end(), event) != events.end();
}

bool IsKnownPermission(const std::string& permission) {
    const auto& permissions = KSMGeneratedContract::Permissions();
    return std::find(
        permissions.begin(),
        permissions.end(),
        permission
    ) != permissions.end();
}

bool IsInitialized() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.initialized;
}

std::shared_ptr<KSMHostServices> GetHostServices() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.host_services;
}

bool IsCommandEnabled(std::string_view command) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.allowed_commands.empty()) {
        return true;
    }
    return std::find(
        state.allowed_commands.begin(),
        state.allowed_commands.end(),
        command
    ) != state.allowed_commands.end();
}

std::optional<Json> InvokePluginData(
    std::string_view command,
    const Json& args
) {
    if (!IsCommandEnabled(command)) {
        return std::nullopt;
    }
    try {
        const Json response = Json::parse(
            KSM::Invoke(command, args.dump()),
            nullptr,
            false
        );
        if (
            response.is_discarded() ||
            !response.value("ok", false) ||
            !response.contains("data")
        ) {
            return std::nullopt;
        }
        return Json(response.at("data"));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string InvalidArgsResponse(const Json& id) {
    return Json{
        {"type", "response"},
        {"id", id},
        {"ok", false},
        {"error", {
            {"code", "INVALID_ARGS"},
            {"message", "args must be valid JSON."}
        }}
    }.dump();
}

}

KSM::PingResult KSM::Phase1::Ping() {
    return KSM::PingResult{.pong = true};
}

KSM::Capabilities KSM::Get() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return KSM::Capabilities{
        .platform = KSM::Platform::Windows,
        .commands = KSMCommandRegistry::SupportedCommands(
            state.allowed_commands
        ),
        .permissions = KSMGeneratedContract::Permissions()
    };
}

KSM::Capabilities KSM::Phase1::QueryCapabilities() {
    return KSM::Get();
}

void KSM::Init(
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services,
    std::vector<std::string> allowed_commands
) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.file_system = std::move(file_system);
    state.host_services = std::move(host_services);
    state.allowed_commands = std::move(allowed_commands);
    state.routers.clear();
    state.initialized = true;
    state.listeners.clear();
}

void KSM::Shutdown() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.routers.clear();
    state.initialized = false;
    state.file_system.reset();
    state.host_services.reset();
    state.allowed_commands.clear();
    state.listeners.clear();
}

std::string KSM::Invoke(
    std::string_view window_id,
    std::string_view command,
    std::string_view args_json
) {
    const auto router = RequireRouter(window_id);
    auto& state = State();
    const std::string id = "req-native-" +
        std::to_string(state.next_request.fetch_add(1));
    const Json parsed_args = Json::parse(args_json, nullptr, false);
    if (parsed_args.is_discarded()) {
        return InvalidArgsResponse(Json(id));
    }

    Json request{
        {"type", "request"},
        {"id", id},
        {"cmd", command},
        {"args", parsed_args}
    };
    if (!window_id.empty()) {
        request["windowId"] = window_id;
    }

    std::string response;
    router->HandleMessage(
        request.dump(),
        [&response](const std::string& value) {
            if (response.empty()) {
                response = value;
            }
        }
    );
    if (response.empty()) {
        throw std::runtime_error("KSM Invoke returned no response.");
    }
    return response;
}

std::string KSM::Invoke(
    std::string_view command,
    std::string_view args_json
) {
    return KSM::Invoke({}, command, args_json);
}

std::uint64_t KSM::On(
    std::string_view event,
    EventHandler handler
) {
    if (!IsCommandEnabled("event.subscribe")) {
        return 0;
    }
    const std::string event_name(event);
    if (!IsKnownEvent(event_name) || !handler) {
        return 0;
    }
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const std::uint64_t subscription = state.next_subscription++;
    state.listeners.emplace(
        subscription,
        std::make_pair(event_name, std::move(handler))
    );
    return subscription;
}

bool KSM::Off(std::uint64_t subscription) {
    if (!IsCommandEnabled("event.unsubscribe")) {
        return false;
    }
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.listeners.erase(subscription) > 0;
}

void KSM::EmitEvent(
    std::string_view event,
    std::string_view payload_json
) {
    const std::string event_name(event);
    if (!IsKnownEvent(event_name)) {
        return;
    }

    std::vector<EventHandler> handlers;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        for (const auto& [subscription, listener] : state.listeners) {
            (void)subscription;
            if (listener.first == event_name) {
                handlers.push_back(listener.second);
            }
        }
    }

    const std::string payload(payload_json);
    for (const auto& handler : handlers) {
        try {
            handler(payload);
        } catch (...) {
            // One native listener must not prevent delivery to the others.
        }
    }
}

std::optional<std::string> KSM::Fs::ReadText(std::string_view path) {
    if (!IsCommandEnabled("fs.readText")) {
        return std::nullopt;
    }
    if (KSMVirtualPath::IsPicked(std::string(path))) {
        const auto host_services = GetHostServices();
        return host_services
            ? host_services->ReadPickedText(std::string(path))
            : std::nullopt;
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system
        ? file_system->ReadText(std::string(path))
        : std::nullopt;
}

bool KSM::Fs::WriteText(
    std::string_view path,
    std::string_view content
) {
    if (!IsCommandEnabled("fs.writeText")) {
        return false;
    }
    if (KSMVirtualPath::IsPicked(std::string(path))) {
        const auto host_services = GetHostServices();
        return host_services &&
            host_services->WritePickedText(
                std::string(path),
                std::string(content)
            );
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system &&
        file_system->WriteText(std::string(path), std::string(content));
}

bool KSM::Fs::Exists(std::string_view path) {
    if (!IsCommandEnabled("fs.exists")) {
        return false;
    }
    if (KSMVirtualPath::IsPicked(std::string(path))) {
        const auto host_services = GetHostServices();
        return host_services &&
            host_services->PickedExists(std::string(path));
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system && file_system->Exists(std::string(path));
}

bool KSM::Fs::Remove(std::string_view path) {
    if (!IsCommandEnabled("fs.remove")) {
        return false;
    }
    if (KSMVirtualPath::IsPicked(std::string(path))) {
        const auto host_services = GetHostServices();
        return host_services &&
            host_services->RemovePicked(std::string(path));
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system && file_system->Remove(std::string(path));
}

std::optional<std::vector<KSMDirectoryEntry>> KSM::Fs::ReadDir(
    std::string_view path
) {
    if (!IsCommandEnabled("fs.readDir")) {
        return std::nullopt;
    }
    const std::string virtual_path(path);
    if (
        !KSMVirtualPath::IsSafeDirectory(virtual_path) ||
        KSMVirtualPath::IsPicked(virtual_path)
    ) {
        return std::nullopt;
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system
        ? file_system->ReadDir(virtual_path)
        : std::nullopt;
}

std::optional<bool> KSM::Fs::MakeDirectory(
    std::string_view path,
    bool recursive
) {
    if (!IsCommandEnabled("fs.mkdir")) {
        return std::nullopt;
    }
    const std::string virtual_path(path);
    if (
        !KSMVirtualPath::IsSafeDirectory(virtual_path) ||
        KSMVirtualPath::IsPicked(virtual_path) ||
        KSMVirtualPath::IsReadOnly(virtual_path)
    ) {
        return std::nullopt;
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system
        ? file_system->MakeDirectory(virtual_path, recursive)
        : std::nullopt;
}

std::optional<KSMFileMetadata> KSM::Fs::Stat(std::string_view path) {
    if (!IsCommandEnabled("fs.stat")) {
        return std::nullopt;
    }
    const std::string virtual_path(path);
    if (!KSMVirtualPath::IsSafeStat(virtual_path)) {
        return std::nullopt;
    }
    if (KSMVirtualPath::IsPicked(virtual_path)) {
        const auto host_services = GetHostServices();
        return host_services
            ? host_services->PickedStat(virtual_path)
            : std::nullopt;
    }
    std::shared_ptr<KSMFileSystem> file_system;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        file_system = state.file_system;
    }
    return file_system
        ? file_system->Stat(virtual_path)
        : std::nullopt;
}

bool KSM::Dialog::Message(
    std::string_view title,
    std::string_view message
) {
    if (!IsCommandEnabled("dialog.message")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->ShowMessage(std::string(title), std::string(message));
}

bool KSM::Dialog::Confirm(
    std::string_view title,
    std::string_view message
) {
    if (!IsCommandEnabled("dialog.confirm")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->Confirm(std::string(title), std::string(message));
}

std::optional<std::string> KSM::Dialog::OpenFile(
    const std::vector<std::string>& filters
) {
    if (!IsCommandEnabled("dialog.openFile")) {
        return std::nullopt;
    }
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->OpenFile(filters)
        : std::nullopt;
}

std::optional<std::string> KSM::Dialog::SaveFile(
    std::string_view suggested_name
) {
    if (!IsCommandEnabled("dialog.saveFile")) {
        return std::nullopt;
    }
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->SaveFile(std::string(suggested_name))
        : std::nullopt;
}

std::optional<std::string> KSM::Clipboard::ReadText() {
    if (!IsCommandEnabled("clipboard.readText")) {
        return std::nullopt;
    }
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->ReadClipboardText()
        : std::nullopt;
}

bool KSM::Clipboard::WriteText(std::string_view text) {
    if (!IsCommandEnabled("clipboard.writeText")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->WriteClipboardText(std::string(text));
}

bool KSM::Notification::Show(
    std::string_view title,
    std::string_view body
) {
    if (!IsCommandEnabled("notification.show")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->ShowNotification(
            std::string(title),
            std::string(body)
        );
}

std::optional<bool> KSM::Permission::Request(std::string_view permission) {
    if (!IsCommandEnabled("permission.request")) {
        return std::nullopt;
    }
    if (!IsInitialized()) {
        return std::nullopt;
    }
    const std::string permission_name(permission);
    if (!IsKnownPermission(permission_name)) {
        return std::nullopt;
    }
    // Windows grants registered permissions through the router state.
    return true;
}

bool KSM::Window::SetTitle(std::string_view title) {
    if (!IsCommandEnabled("window.setTitle")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->SetWindowTitle(std::string(title));
}

bool KSM::Window::Minimize() {
    if (!IsCommandEnabled("window.minimize")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services && host_services->MinimizeWindow();
}

bool KSM::Window::Maximize() {
    if (!IsCommandEnabled("window.maximize")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services && host_services->MaximizeWindow();
}

bool KSM::Window::SetSize(const KSMWindowSize& size) {
    if (!IsCommandEnabled("window.setSize")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services && host_services->SetWindowSize(size);
}

bool KSM::Window::Close() {
    if (!IsCommandEnabled("window.close")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services && host_services->CloseWindow();
}

std::optional<KSMHttpDownloadResult> KSM::Http::Download(
    std::string_view url,
    std::string_view path,
    bool overwrite
) {
    if (!IsCommandEnabled("http.download")) {
        return std::nullopt;
    }
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->Download(
            std::string(url),
            std::string(path),
            overwrite,
            ""
        )
        : std::nullopt;
}

bool KSM::Shell::OpenExternal(std::string_view url) {
    if (!IsCommandEnabled("shell.openExternal")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->IsExternalUrlAllowed(std::string(url)) &&
        host_services->OpenExternal(std::string(url));
}

std::optional<KSMDeviceInfo> KSM::Device::GetInfo() {
    if (!IsCommandEnabled("device.getInfo")) {
        return std::nullopt;
    }
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->GetDeviceInfo()
        : std::nullopt;
}

std::optional<KSM::PathDirectories> KSM::Path::Dirs() {
    const auto data = InvokePluginData("path.dirs", Json::object());
    if (!data) {
        return std::nullopt;
    }
    return PathDirectories{
        .data = data->value("data", std::string()),
        .cache = data->value("cache", std::string()),
        .temp = data->value("temp", std::string()),
        .resource = data->value("resource", std::string()),
        .asset = data->value("asset", std::string())
    };
}

std::optional<std::string> KSM::Path::Join(
    std::string_view base,
    const std::vector<std::string>& segments
) {
    const auto data = InvokePluginData(
        "path.join",
        Json{
            {"base", std::string(base)},
            {"segments", segments}
        }
    );
    return data
        ? std::optional<std::string>(data->value("path", std::string()))
        : std::nullopt;
}

std::optional<std::string> KSM::Path::Normalize(std::string_view path) {
    const auto data = InvokePluginData(
        "path.normalize",
        Json{{"path", std::string(path)}}
    );
    return data
        ? std::optional<std::string>(data->value("path", std::string()))
        : std::nullopt;
}

std::optional<std::string> KSM::Path::Dirname(std::string_view path) {
    const auto data = InvokePluginData(
        "path.dirname",
        Json{{"path", std::string(path)}}
    );
    return data
        ? std::optional<std::string>(data->value("path", std::string()))
        : std::nullopt;
}

std::optional<std::string> KSM::Path::Basename(
    std::string_view path,
    std::string_view suffix
) {
    Json args{{"path", std::string(path)}};
    if (!suffix.empty()) {
        args["suffix"] = std::string(suffix);
    }
    const auto data = InvokePluginData("path.basename", args);
    return data
        ? std::optional<std::string>(data->value("name", std::string()))
        : std::nullopt;
}

std::optional<std::string> KSM::Path::Extname(std::string_view path) {
    const auto data = InvokePluginData(
        "path.extname",
        Json{{"path", std::string(path)}}
    );
    return data
        ? std::optional<std::string>(
            data->value("extension", std::string())
        )
        : std::nullopt;
}

std::optional<bool> KSM::Path::IsAbsolute(std::string_view path) {
    const auto data = InvokePluginData(
        "path.isAbsolute",
        Json{{"path", std::string(path)}}
    );
    return data
        ? std::optional<bool>(data->value("absolute", false))
        : std::nullopt;
}

std::optional<KSM::StoreGetResult> KSM::Store::Get(std::string_view key) {
    const auto data = InvokePluginData(
        "store.get",
        Json{{"key", std::string(key)}}
    );
    if (!data || !data->contains("value")) {
        return std::nullopt;
    }
    return StoreGetResult{
        .found = data->value("found", false),
        .value_json = data->at("value").dump()
    };
}

bool KSM::Store::Set(
    std::string_view key,
    std::string_view value_json
) {
    const Json value = Json::parse(std::string(value_json), nullptr, false);
    if (value.is_discarded()) {
        return false;
    }
    const auto data = InvokePluginData(
        "store.set",
        Json{
            {"key", std::string(key)},
            {"value", value}
        }
    );
    return data && data->value("updated", false);
}

std::optional<bool> KSM::Store::Delete(std::string_view key) {
    const auto data = InvokePluginData(
        "store.delete",
        Json{{"key", std::string(key)}}
    );
    return data
        ? std::optional<bool>(data->value("deleted", false))
        : std::nullopt;
}

bool KSM::Store::Clear() {
    const auto data = InvokePluginData("store.clear", Json::object());
    return data && data->value("cleared", false);
}

std::optional<std::vector<std::string>> KSM::Store::Keys() {
    const auto data = InvokePluginData("store.keys", Json::object());
    if (!data || !data->contains("keys") || !data->at("keys").is_array()) {
        return std::nullopt;
    }
    return data->at("keys").get<std::vector<std::string>>();
}

std::optional<KSM::OperatingSystemInfo> KSM::Os::GetInfo() {
    const auto data = InvokePluginData("os.getInfo", Json::object());
    if (!data) {
        return std::nullopt;
    }
    return OperatingSystemInfo{
        .platform = data->value("platform", std::string()),
        .architecture = data->value("architecture", std::string()),
        .version = data->value("version", std::string()),
        .locale = data->value("locale", std::string()),
        .timezone = data->value("timezone", std::string()),
        .app_version = data->value("appVersion", std::string())
    };
}

namespace {

std::string_view LogLevelName(KSM::LogLevel level) {
    switch (level) {
        case KSM::LogLevel::Trace:
            return "trace";
        case KSM::LogLevel::Debug:
            return "debug";
        case KSM::LogLevel::Info:
            return "info";
        case KSM::LogLevel::Warn:
            return "warn";
        case KSM::LogLevel::Error:
            return "error";
    }
    return "info";
}

} // namespace

bool KSM::Log::Write(
    LogLevel level,
    std::string_view message,
    std::string_view target
) {
    Json args{
        {"level", std::string(LogLevelName(level))},
        {"message", std::string(message)}
    };
    if (!target.empty()) {
        args["target"] = std::string(target);
    }
    const auto data = InvokePluginData("log.write", args);
    return data && data->value("logged", false);
}

std::optional<KSM::AppInfo> KSM::App::GetInfo() {
    if (!IsCommandEnabled("app.getInfo")) {
        return std::nullopt;
    }
    const auto host_services = GetHostServices();
    if (!host_services) {
        return std::nullopt;
    }
    return KSM::AppInfo{
        .name = "Kasumi",
        .version = host_services->AppVersion(),
        .platform = KSM::Platform::Windows,
        .language = host_services->Language()
    };
}

bool KSM::App::Exit() {
    if (!IsCommandEnabled("app.exit")) {
        return false;
    }
    const auto host_services = GetHostServices();
    return host_services && host_services->ExitApp();
}
