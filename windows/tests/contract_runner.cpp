#include <nlohmann/json.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../src/bridge/ksm_router.h"
#include "../src/commands/ksm_filesystem.h"
#include "../src/commands/ksm_phase1.h"
#include "../src/platform/ksm_host_services.h"
#include "../src/platform/ksm_url_policy.h"
#include "../src/platform/ksm_webview_security.h"
#include "../src/platform/ksm_webview_host.h"

namespace {

using Json = nlohmann::json;

Json Invoke(KSMRouter& router, const std::string& command, const Json& args) {
    const Json request{
        {"type", "request"},
        {"id", "contract"},
        {"cmd", command},
        {"args", args}
    };
    std::string response_text;
    router.HandleMessage(request.dump(), [&response_text](const std::string& response) {
        response_text = response;
    });
    if (response_text.empty()) {
        throw std::runtime_error("The router returned no response.");
    }
    return Json::parse(response_text);
}

void ExpectOk(const Json& response) {
    if (!response.value("ok", false)) {
        throw std::runtime_error(response.dump());
    }
}

void ExpectError(const Json& response, const std::string& code) {
    if (response.value("ok", true)) {
        throw std::runtime_error("Expected an error response.");
    }
    const std::string actual = response.at("error").value("code", "");
    if (actual != code) {
        throw std::runtime_error(
            "Expected " + code + " but received " + actual + "."
        );
    }
}

class ResponseQueue {
public:
    void Push(const std::string& response) {
        {
            std::lock_guard lock(mutex_);
            responses_.push_back(response);
        }
        ready_.notify_one();
    }

    std::optional<Json> WaitFor(
        const std::function<bool(const Json&)>& predicate,
        std::chrono::milliseconds timeout
    ) {
        std::unique_lock lock(mutex_);
        if (!ready_.wait_for(lock, timeout, [this, &predicate] {
            return std::any_of(
                responses_.begin(),
                responses_.end(),
                [&predicate](const std::string& response) {
                    return predicate(Json::parse(response));
                }
            );
        })) {
            return std::nullopt;
        }
        for (auto iterator = responses_.begin(); iterator != responses_.end(); ++iterator) {
            const Json response = Json::parse(*iterator);
            if (predicate(response)) {
                responses_.erase(iterator);
 return std::optional<Json>(response);
            }
        }
        return std::nullopt;
    }

    bool Empty() {
        std::lock_guard lock(mutex_);
        return responses_.empty();
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::string> responses_;
};

class BlockingHostServices final : public KSMHostServices {
public:
    std::string AppVersion() const override {
        return "0.1.0";
    }

    std::string Language() const override {
        return "en-US";
    }

    bool ShowMessage(
        const std::string&,
        const std::string&
    ) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return true;
    }

    bool Confirm(
        const std::string&,
        const std::string&
    ) override {
        return ShowMessage("", "");
    }

    std::optional<std::string> OpenFile(
        const std::vector<std::string>&
    ) override {
        return std::nullopt;
    }

    std::optional<std::string> SaveFile(
        const std::string&
    ) override {
        return std::nullopt;
    }

    std::optional<std::string> ReadPickedText(
        const std::string& path
    ) override {
        return path == "app://picked/contract"
            ? std::optional<std::string>("picked")
            : std::nullopt;
    }

    bool WritePickedText(
        const std::string& path,
        const std::string& content
    ) override {
        return path == "app://picked/contract" && content == "updated";
    }

    bool PickedExists(const std::string& path) override {
        return path == "app://picked/contract";
    }

    std::optional<KSMFileMetadata> PickedStat(
        const std::string& path
    ) override {
        if (path != "app://picked/contract") {
            return std::nullopt;
        }
        return KSMFileMetadata{KSMFileKind::File, 6, std::nullopt};
    }

    bool RemovePicked(const std::string& path) override {
        return path == "app://picked/contract";
    }

    std::optional<std::string> ReadClipboardText() override {
        return "clipboard";
    }

    bool WriteClipboardText(const std::string& text) override {
        return text == "updated";
    }

    bool ShowNotification(
        const std::string& title,
        const std::string& body
    ) override {
        return title == "Kasumi" && body == "root";
    }

    bool SetWindowTitle(const std::string& title) override {
        return title == "Root";
    }

    bool MinimizeWindow() override {
        return true;
    }

    bool MaximizeWindow() override {
        return true;
    }

    bool CloseWindow() override {
        return true;
    }

    bool ExitApp() override {
        return true;
    }

    std::optional<KSMHttpDownloadResult> Download(
        const std::string& url,
        const std::string& path,
        bool overwrite,
        const std::string& request_id
    ) override {
        if (
            url != "https://example.com/file.bin" ||
            path != "app://data/file.bin" ||
            overwrite ||
            request_id != "contract"
        ) {
            return std::nullopt;
        }
        return KSMHttpDownloadResult{path, 42};
    }

    bool IsExternalUrlAllowed(const std::string& url) const override {
        return url.starts_with("https://example.com/");
    }

    bool OpenExternal(const std::string& url) override {
        return IsExternalUrlAllowed(url);
    }

    bool SetWindowSize(const KSMWindowSize& size) override {
        return size.width >= 320 && size.height >= 240;
    }
};

bool IsResponse(const Json& response, const std::string& id) {
    return response.value("type", "") == "response" &&
        response.value("id", "") == id;
}

void TestHostTimeoutAndCancel(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    ResponseQueue responses;
    auto services = std::make_shared<BlockingHostServices>();
    KSMWebViewHost host(
        [&responses](const std::string& response) {
            responses.Push(response);
        },
        file_system,
        services
    );

    host.OnMessageReceived(Json{
        {"type", "request"},
        {"id", "timeout-request"},
        {"cmd", "dialog.message"},
        {"args", {{"title", "Kasumi"}, {"message", "wait"}}},
        {"timeoutMs", 10}
    }.dump());
    const auto timeout = responses.WaitFor(
        [](const Json& response) {
            return IsResponse(response, "timeout-request");
        },
        std::chrono::seconds(1)
    );
    if (!timeout) {
        throw std::runtime_error("The host did not return a timeout response.");
    }
    ExpectError(*timeout, "TIMEOUT");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (!responses.Empty()) {
        throw std::runtime_error("A timed out request returned a second response.");
    }

    host.OnMessageReceived(Json{
        {"type", "request"},
        {"id", "cancel-request"},
        {"cmd", "dialog.message"},
        {"args", {{"title", "Kasumi"}, {"message", "wait"}}},
        {"timeoutMs", 5000}
    }.dump());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    host.OnMessageReceived(Json{
        {"type", "cancel"},
        {"id", "cancel-request"}
    }.dump());
    const auto cancelled = responses.WaitFor(
        [](const Json& response) {
            return IsResponse(response, "cancel-request");
        },
        std::chrono::seconds(1)
    );
    if (!cancelled) {
        throw std::runtime_error("The host did not return a cancellation response.");
    }
    ExpectError(*cancelled, "CANCELLED");

    host.NotifyWebViewReloaded();
    const auto reloaded = responses.WaitFor(
        [](const Json& response) {
            return response.value("type", "") == "event" &&
                response.value("event", "") == "webview.reloaded";
        },
        std::chrono::seconds(1)
    );
    if (!reloaded) {
        throw std::runtime_error("The host did not emit webview.reloaded.");
    }
}

void TestHostShutdownDropsResponses(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    ResponseQueue responses;
    {
        auto services = std::make_shared<BlockingHostServices>();
        KSMWebViewHost host(
            [&responses](const std::string& response) {
                responses.Push(response);
            },
            file_system,
            services
        );
        host.OnMessageReceived(Json{
            {"type", "request"},
            {"id", "shutdown-request"},
            {"cmd", "dialog.message"},
            {"args", {{"title", "Kasumi"}, {"message", "wait"}}},
            {"timeoutMs", 5000}
        }.dump());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (!responses.Empty()) {
        throw std::runtime_error(
            "A destroyed WebView host delivered a late response."
        );
    }
}

void TestKsmRoot(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    auto services = std::make_shared<BlockingHostServices>();
    KSM::Init(file_system, services);

    const auto ping = Json::parse(KSM::Invoke("window-1", "ping", "{}"));
    ExpectOk(ping);
    const auto second_ping = Json::parse(
        KSM::Invoke("window-2", "ping", "{}")
    );
    ExpectOk(second_ping);
    const std::string request_id = ping.value("id", "");
    if (
        !request_id.starts_with("req-native-") ||
        ping.at("data").at("pong") != true ||
        second_ping.value("id", "") == request_id
    ) {
        throw std::runtime_error(
            "KSM::Invoke did not route independent window requests."
        );
    }

    if (!KSM::Fs::WriteText("app://data/root.txt", "root")) {
        throw std::runtime_error("KSM::Fs::WriteText failed.");
    }
    if (KSM::Fs::ReadText("app://data/root.txt") != "root") {
        throw std::runtime_error("KSM::Fs::ReadText returned unexpected content.");
    }
    if (KSM::Fs::ReadText("app://picked/contract") != "picked") {
        throw std::runtime_error("KSM::Fs::ReadText did not use picked files.");
    }
    if (
        !KSM::Fs::Exists("app://picked/contract") ||
        !KSM::Fs::WriteText("app://picked/contract", "updated") ||
        !KSM::Fs::Remove("app://picked/contract")
    ) {
        throw std::runtime_error("KSM::Fs picked operations failed.");
    }
    const auto picked_stat = KSM::Fs::Stat("app://picked/contract");
    if (
        !picked_stat ||
        picked_stat->kind != KSMFileKind::File ||
        picked_stat->size != 6
    ) {
        throw std::runtime_error("KSM::Fs::Stat failed for picked files.");
    }
    const auto directory_created = KSM::Fs::MakeDirectory(
        "app://data/root-dir",
        true
    );
    if (!directory_created || !*directory_created) {
        throw std::runtime_error("KSM::Fs::MakeDirectory failed.");
    }
    const auto directory_stat = KSM::Fs::Stat("app://data/root-dir");
    const auto directory_entries = KSM::Fs::ReadDir("app://data");
    if (
        !directory_stat ||
        directory_stat->kind != KSMFileKind::Directory ||
        !directory_entries
    ) {
        throw std::runtime_error("KSM::Fs directory operations failed.");
    }
    if (!KSM::Dialog::Message("Kasumi", "root")) {
        throw std::runtime_error("KSM::Dialog::Message failed.");
    }
    if (KSM::Clipboard::ReadText() != "clipboard") {
        throw std::runtime_error("KSM::Clipboard::ReadText failed.");
    }
    if (!KSM::Clipboard::WriteText("updated")) {
        throw std::runtime_error("KSM::Clipboard::WriteText failed.");
    }
    if (!KSM::Notification::Show("Kasumi", "root")) {
        throw std::runtime_error("KSM::Notification::Show failed.");
    }
    if (KSM::Permission::Request("clipboard.read") != true) {
        throw std::runtime_error("KSM::Permission::Request failed.");
    }
    if (KSM::Permission::Request("unknown.permission").has_value()) {
        throw std::runtime_error("KSM::Permission::Request accepted an unknown permission.");
    }
    if (!KSM::Window::SetTitle("Root") ||
        !KSM::Window::Minimize() ||
        !KSM::Window::Maximize() ||
        !KSM::Window::Close()) {
        throw std::runtime_error("KSM::Window failed.");
    }
    const auto app_info = KSM::App::GetInfo();
    if (
        !app_info ||
        app_info->name != "Kasumi" ||
        app_info->version != "0.1.0" ||
        app_info->platform != KSM::Platform::Windows ||
        app_info->language != "en-US"
    ) {
        throw std::runtime_error("KSM::App::GetInfo failed.");
    }
    if (!KSM::App::Exit()) {
        throw std::runtime_error("KSM::App::Exit failed.");
    }

    bool event_received = false;
    const auto subscription = KSM::On(
        "window.resized",
        [&event_received](const std::string& payload) {
            event_received = payload == R"({"width":800,"height":600})";
        }
    );
    if (subscription == 0) {
        throw std::runtime_error("KSM::On did not register a listener.");
    }
    KSM::EmitEvent("window.resized", R"({"width":800,"height":600})");
    if (!event_received || !KSM::Off(subscription)) {
        throw std::runtime_error("KSM native event delivery failed.");
    }

    KSM::Shutdown();
    if (KSM::Fs::ReadText("app://data/root.txt").has_value()) {
        throw std::runtime_error("KSM::Shutdown left the runtime active.");
    }
    if (KSM::Fs::ReadText("app://picked/contract").has_value()) {
        throw std::runtime_error("KSM::Shutdown left picked files active.");
    }
    if (
        KSM::Fs::Stat("app://data/root-dir").has_value() ||
        KSM::Fs::ReadDir("app://data").has_value()
    ) {
        throw std::runtime_error("KSM::Shutdown left directory APIs active.");
    }
    if (KSM::Dialog::Message("Kasumi", "root") ||
        KSM::Clipboard::ReadText().has_value() ||
        KSM::Clipboard::WriteText("updated") ||
        KSM::Notification::Show("Kasumi", "root") ||
        KSM::Permission::Request("clipboard.read").has_value() ||
        KSM::Window::Close() ||
        KSM::App::GetInfo().has_value() ||
        KSM::App::Exit()) {
        throw std::runtime_error("KSM::Shutdown left a submodule active.");
    }
}

void TestWebViewSecurity() {
    for (const auto& origin : {
        "https://app.local",
        "https://app.local/index.html",
        "http://localhost:5173",
        "http://localhost:5173/index.html"
    }) {
        if (!KSMWebViewSecurity::IsAllowedOrigin(origin)) {
            throw std::runtime_error(
                std::string("A trusted origin was rejected: ") + origin
            );
        }
    }
    for (const auto& origin : {
        "https://app.local.evil.example",
        "https://app.local:444",
        "http://localhost:5173.evil.example",
        "file:///index.html",
        "javascript:alert(1)"
    }) {
        if (KSMWebViewSecurity::IsAllowedOrigin(origin)) {
            throw std::runtime_error(
                std::string("An untrusted origin was accepted: ") + origin
            );
        }
    }
}

void TestUrlPolicy() {
    const auto parsed = KSMUrlPolicy::ParseHttpUrl(
        "https://Example.com:443/path?q=1",
        true
    );
    if (
        !parsed ||
        parsed->origin.scheme != "https" ||
        parsed->origin.host != "example.com" ||
        parsed->origin.port != 443 ||
        parsed->path != "/path" ||
        parsed->query != "?q=1"
    ) {
        throw std::runtime_error("The URL policy did not normalize an HTTPS URL.");
    }
    for (const auto& value : {
        "http://example.com/file",
        "https://user@example.com/file",
        "https://example.com:0/file",
        "file:///tmp/file",
        "javascript:alert(1)"
    }) {
        if (KSMUrlPolicy::ParseHttpUrl(value, true)) {
            throw std::runtime_error(
                std::string("The URL policy accepted an invalid URL: ") + value
            );
        }
    }
    const auto redirect = KSMUrlPolicy::ResolveHttpsRedirect(
        "https://example.com/a/b?old=1",
        "../c?new=1"
    );
    if (
        !redirect ||
        *redirect != "https://example.com/c?new=1"
    ) {
        throw std::runtime_error("The URL policy did not resolve a relative redirect.");
    }
    if (KSMUrlPolicy::ResolveHttpsRedirect(
        "https://example.com/a",
        "http://example.com/b"
    )) {
        throw std::runtime_error("The URL policy accepted an HTTP redirect.");
    }
}

void TestNewCommands(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    auto services = std::make_shared<BlockingHostServices>();
    KSMRouter router(file_system, services);

    const auto download = Invoke(
        router,
        "http.download",
        Json{
            {"url", "https://example.com/file.bin"},
            {"path", "app://data/file.bin"}
        }
    );
    ExpectOk(download);
    if (
        download.at("data").at("path") != "app://data/file.bin" ||
        download.at("data").at("bytes") != 42
    ) {
        throw std::runtime_error("http.download returned unexpected data.");
    }
    ExpectError(
        Invoke(
            router,
            "http.download",
            Json{
                {"url", "http://example.com/file.bin"},
                {"path", "app://data/file.bin"}
            }
        ),
        "INVALID_ARGS"
    );
    ExpectError(
        Invoke(
            router,
            "http.download",
            Json{
                {"url", "https://example.com/file.bin"},
                {"path", "app://picked/file.bin"}
            }
        ),
        "PERMISSION_DENIED"
    );
    ExpectError(
        Invoke(
            router,
            "http.download",
            Json{
                {"url", "https://example.com/file.bin"},
                {"path", "app://data/file.bin"},
                {"overwrite", "yes"}
            }
        ),
        "INVALID_ARGS"
    );

    ExpectOk(Invoke(
        router,
        "shell.openExternal",
        Json{{"url", "https://example.com/docs"}}
    ));
    ExpectError(
        Invoke(
            router,
            "shell.openExternal",
            Json{{"url", "https://other.example/docs"}}
        ),
        "PERMISSION_DENIED"
    );
    ExpectError(
        Invoke(
            router,
            "shell.openExternal",
            Json{{"url", "file:///tmp/index.html"}}
        ),
        "INVALID_ARGS"
    );

    ExpectOk(Invoke(
        router,
        "window.setSize",
        Json{{"width", 800}, {"height", 600}}
    ));
    ExpectError(
        Invoke(
            router,
            "window.setSize",
            Json{{"width", 319}, {"height", 600}}
        ),
        "INVALID_ARGS"
    );
    ExpectError(
        Invoke(
            router,
            "window.setSize",
            Json{{"width", 800.5}, {"height", 600}}
        ),
        "INVALID_ARGS"
    );
    ExpectError(
        Invoke(router, "device.getInfo", Json::object()),
        "NOT_SUPPORTED"
    );
}

}

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("kasumi-contract-" + std::to_string(suffix));
    const auto resource_root = root / "res";
    const auto asset_root = root / "assets";
    std::filesystem::create_directories(resource_root);
    std::filesystem::create_directories(asset_root);
    {
        std::ofstream resource(resource_root / "contract.txt", std::ios::binary);
        resource << "resource";
        std::ofstream asset(asset_root / "contract.txt", std::ios::binary);
        asset << "asset";
    }

    auto file_system = std::make_shared<KSMFileSystem>(
        root / "data",
        root / "cache",
        root / "tmp",
        resource_root,
        asset_root
    );
    KSMRouter router(file_system);

    try {
        const auto ping = Invoke(router, "ping", Json::object());
        ExpectOk(ping);
        if (ping.at("data").at("pong") != true) {
            throw std::runtime_error("ping did not return pong.");
        }

        const auto capabilities = Invoke(router, "capabilities.query", Json::object());
        ExpectOk(capabilities);
        if (capabilities.at("data").at("platform") != "windows") {
            throw std::runtime_error("capabilities.query returned the wrong platform.");
        }
        if (capabilities.at("data").at("commands").empty()) {
            throw std::runtime_error("capabilities.query returned no commands.");
        }

        ExpectError(
            Invoke(router, "unknown.command", Json::object()),
            "UNKNOWN_COMMAND"
        );
        ExpectError(
            Invoke(router, "fs.writeText", Json{{"path", "app://data/x.txt"}}),
            "INVALID_ARGS"
        );
        ExpectError(
            Invoke(
                router,
                "fs.readText",
                Json{{"path", "app://data/../secret.txt"}}
            ),
            "PERMISSION_DENIED"
        );
        for (const auto& path : {
            "app://data/C:/secret.txt",
            "app://data/C:secret.txt",
            "app://data/file.txt:stream",
            "app://data/\\server\\share",
            "app://data//server",
            "app://picked/1/2",
            "app://other/file.txt",
            "file:///secret.txt"
        }) {
            ExpectError(
                Invoke(router, "fs.readText", Json{{"path", path}}),
                "PERMISSION_DENIED"
            );
        }
        ExpectError(
            Invoke(router, "event.subscribe", Json{{"event", "unknown.event"}}),
            "INVALID_ARGS"
        );
        ExpectError(
            Invoke(
                router,
                "permission.request",
                Json{{"permission", "unknown.permission"}}
            ),
            "INVALID_ARGS"
        );

        ExpectOk(Invoke(
            router,
            "fs.writeText",
            Json{{"path", "app://data/contract.txt"}, {"content", "kasumi"}}
        ));
        const auto read = Invoke(
            router,
            "fs.readText",
            Json{{"path", "app://data/contract.txt"}}
        );
        ExpectOk(read);
        if (!read.at("data").is_string() || read.at("data") != "kasumi") {
            throw std::runtime_error("fs.readText returned unexpected content.");
        }
        ExpectOk(Invoke(
            router,
            "fs.remove",
            Json{{"path", "app://data/contract.txt"}}
        ));

        const auto created = Invoke(
            router,
            "fs.mkdir",
            Json{
                {"path", "app://data/contract-dir"},
                {"recursive", true}
            }
        );
        ExpectOk(created);
        if (created.at("data").at("created") != true) {
            throw std::runtime_error("fs.mkdir did not create a directory.");
        }
        const auto created_again = Invoke(
            router,
            "fs.mkdir",
            Json{
                {"path", "app://data/contract-dir"},
                {"recursive", true}
            }
        );
        ExpectOk(created_again);
        if (created_again.at("data").at("created") != false) {
            throw std::runtime_error("fs.mkdir recreated a directory.");
        }
        ExpectOk(Invoke(
            router,
            "fs.writeText",
            Json{
                {"path", "app://data/contract-dir/nested.txt"},
                {"content", "nested"}
            }
        ));
        const auto directory_stat = Invoke(
            router,
            "fs.stat",
            Json{{"path", "app://data/contract-dir"}}
        );
        ExpectOk(directory_stat);
        if (
            directory_stat.at("data").at("kind") != "directory" ||
            directory_stat.at("data").at("size") != 0
        ) {
            throw std::runtime_error("fs.stat returned invalid directory metadata.");
        }
        const auto file_stat = Invoke(
            router,
            "fs.stat",
            Json{{"path", "app://data/contract-dir/nested.txt"}}
        );
        ExpectOk(file_stat);
        if (
            file_stat.at("data").at("kind") != "file" ||
            file_stat.at("data").at("size") != 6
        ) {
            throw std::runtime_error("fs.stat returned invalid file metadata.");
        }
        const auto listing = Invoke(
            router,
            "fs.readDir",
            Json{{"path", "app://data"}}
        );
        ExpectOk(listing);
        const auto& entries = listing.at("data").at("entries");
        const bool found_directory = std::any_of(
            entries.begin(),
            entries.end(),
            [](const Json& entry) {
                return entry.at("name") == "contract-dir" &&
                    entry.at("kind") == "directory";
            }
        );
        if (!found_directory) {
            throw std::runtime_error("fs.readDir did not list the directory.");
        }
        const auto nested_listing = Invoke(
            router,
            "fs.readDir",
            Json{{"path", "app://data/contract-dir"}}
        );
        ExpectOk(nested_listing);
        if (
            nested_listing.at("data").at("entries").size() != 1 ||
            nested_listing.at("data").at("entries").at(0).at("name") !=
                "nested.txt"
        ) {
            throw std::runtime_error("fs.readDir did not list directory contents.");
        }
        const auto resource_stat = Invoke(
            router,
            "fs.stat",
            Json{{"path", "app://res/contract.txt"}}
        );
        ExpectOk(resource_stat);
        if (resource_stat.at("data").at("kind") != "file") {
            throw std::runtime_error("fs.stat did not read resource metadata.");
        }
        const auto resource_listing = Invoke(
            router,
            "fs.readDir",
            Json{{"path", "app://res"}}
        );
        ExpectOk(resource_listing);
        if (resource_listing.at("data").at("entries").empty()) {
            throw std::runtime_error("fs.readDir did not list resources.");
        }
        ExpectOk(Invoke(
            router,
            "fs.remove",
            Json{{"path", "app://data/contract-dir/nested.txt"}}
        ));
        ExpectOk(Invoke(
            router,
            "fs.remove",
            Json{{"path", "app://data/contract-dir"}}
        ));

        const auto resource = Invoke(
            router,
            "fs.readText",
            Json{{"path", "app://res/contract.txt"}}
        );
        ExpectOk(resource);
        if (resource.at("data") != "resource") {
            throw std::runtime_error("fs.readText did not read app://res.");
        }
        const auto asset = Invoke(
            router,
            "fs.readText",
            Json{{"path", "app://assets/contract.txt"}}
        );
        ExpectOk(asset);
        if (asset.at("data") != "asset") {
            throw std::runtime_error("fs.readText did not read app://assets.");
        }
        ExpectError(
            Invoke(
                router,
                "fs.writeText",
                Json{{"path", "app://res/contract.txt"}, {"content", "blocked"}}
            ),
            "PERMISSION_DENIED"
        );
        ExpectError(
            Invoke(
                router,
                "fs.writeText",
                Json{{"path", "app://assets/contract.txt"}, {"content", "blocked"}}
            ),
            "PERMISSION_DENIED"
        );

        const std::string large_content(70 * 1024, 'x');
        if (!file_system->WriteText("app://data/large.txt", large_content)) {
            throw std::runtime_error("Could not create the large test file.");
        }
        const auto large = Invoke(
            router,
            "fs.readText",
            Json{{"path", "app://data/large.txt"}}
        );
        ExpectOk(large);
        if (!large.at("data").is_object() || !large.at("data").contains("file")) {
            throw std::runtime_error("Large text did not return a file handle.");
        }
        const std::string temporary = large.at("data").at("file").get<std::string>();
        if (!temporary.starts_with("app://tmp/") || !file_system->Exists(temporary)) {
            throw std::runtime_error("Large text handle does not point to app://tmp.");
        }
        const auto large_again = Invoke(
            router,
            "fs.readText",
            Json{{"path", temporary}}
        );
        ExpectOk(large_again);
        if (
            !large_again.at("data").is_object() ||
            large_again.at("data").at("file") != temporary
        ) {
            throw std::runtime_error("Large temporary handle was not stable.");
        }

        ExpectError(
            Invoke(
                router,
                "fs.writeText",
                Json{{"path", "app://data/large.txt"}, {"content", large_content}}
            ),
            "INVALID_ARGS"
        );

        TestHostTimeoutAndCancel(file_system);
        TestHostShutdownDropsResponses(file_system);
        TestKsmRoot(file_system);
        TestWebViewSecurity();
        TestUrlPolicy();
        TestNewCommands(file_system);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        return 1;
    }

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::cout << "windows contract passed\n";
    return 0;
}
