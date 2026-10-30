#include "ksm_phase1.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

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
        state.host_services
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

const KSM::Capabilities& KSM::Get() {
    static const KSM::Capabilities capabilities{
        .platform = KSM::Platform::Windows,
        .commands = KSMGeneratedContract::WindowsCommands(),
        .permissions = KSMGeneratedContract::Permissions()
    };
    return capabilities;
}

KSM::Capabilities KSM::Phase1::QueryCapabilities() {
    return KSM::Get();
}

void KSM::Init(
    std::shared_ptr<KSMFileSystem> file_system,
    std::shared_ptr<KSMHostServices> host_services
) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.file_system = std::move(file_system);
    state.host_services = std::move(host_services);
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
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->ShowMessage(std::string(title), std::string(message));
}

bool KSM::Dialog::Confirm(
    std::string_view title,
    std::string_view message
) {
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->Confirm(std::string(title), std::string(message));
}

std::optional<std::string> KSM::Dialog::OpenFile(
    const std::vector<std::string>& filters
) {
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->OpenFile(filters)
        : std::nullopt;
}

std::optional<std::string> KSM::Dialog::SaveFile(
    std::string_view suggested_name
) {
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->SaveFile(std::string(suggested_name))
        : std::nullopt;
}

std::optional<std::string> KSM::Clipboard::ReadText() {
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->ReadClipboardText()
        : std::nullopt;
}

bool KSM::Clipboard::WriteText(std::string_view text) {
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->WriteClipboardText(std::string(text));
}

bool KSM::Notification::Show(
    std::string_view title,
    std::string_view body
) {
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->ShowNotification(
            std::string(title),
            std::string(body)
        );
}

std::optional<bool> KSM::Permission::Request(std::string_view permission) {
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
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->SetWindowTitle(std::string(title));
}

bool KSM::Window::Minimize() {
    const auto host_services = GetHostServices();
    return host_services && host_services->MinimizeWindow();
}

bool KSM::Window::Maximize() {
    const auto host_services = GetHostServices();
    return host_services && host_services->MaximizeWindow();
}

bool KSM::Window::SetSize(const KSMWindowSize& size) {
    const auto host_services = GetHostServices();
    return host_services && host_services->SetWindowSize(size);
}

bool KSM::Window::Close() {
    const auto host_services = GetHostServices();
    return host_services && host_services->CloseWindow();
}

std::optional<KSMHttpDownloadResult> KSM::Http::Download(
    std::string_view url,
    std::string_view path,
    bool overwrite
) {
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
    const auto host_services = GetHostServices();
    return host_services &&
        host_services->IsExternalUrlAllowed(std::string(url)) &&
        host_services->OpenExternal(std::string(url));
}

std::optional<KSMDeviceInfo> KSM::Device::GetInfo() {
    const auto host_services = GetHostServices();
    return host_services
        ? host_services->GetDeviceInfo()
        : std::nullopt;
}

std::optional<KSM::AppInfo> KSM::App::GetInfo() {
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
    const auto host_services = GetHostServices();
    return host_services && host_services->ExitApp();
}
