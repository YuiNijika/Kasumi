#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
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
#include "../src/platform/ksm_sql_driver.h"
#include "../src/platform/ksm_sqlite_driver.h"
#include "../src/platform/ksm_url_policy.h"
#include "../src/platform/ksm_webview_security.h"
#include "../src/platform/ksm_webview_host.h"
#include "../src/platform/ksm_windows_host_services.h"
#include "../src/plugins/ksm_log_plugin.h"
#include "../src/plugins/ksm_os_plugin.h"
#include "../src/plugins/ksm_path_plugin.h"
#include "../src/plugins/ksm_store_plugin.h"
#include "ksm_version.h"

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
        return KASUMI_VERSION_STRING;
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

    std::optional<KSMOperatingSystemInfo> GetOperatingSystemInfo() const override {
        return KSMOperatingSystemInfo{
            "windows",
            "x86_64",
            "10.0.26100",
            "en-US",
            "Asia/Shanghai"
        };
    }

    bool WriteLog(
        const std::string& level,
        const std::string& target,
        const std::string& message
    ) override {
        logged_level = level;
        logged_target = target;
        logged_message = message;
        return true;
    }

    mutable std::string logged_level;
    mutable std::string logged_target;
    mutable std::string logged_message;
};

class ContractEchoPlugin final : public KSMPlugin {
public:
    const std::string& Id() const override {
        static const std::string id = "contract.echo";
        return id;
    }

    const std::vector<std::string>& Commands() const override {
        static const std::vector<std::string> commands = {
            "contract.echo"
        };
        return commands;
    }

    const std::vector<std::string>& Permissions() const override {
        static const std::vector<std::string> permissions = {
            "contract.echo"
        };
        return permissions;
    }

    bool ValidateArgs(
        const std::string& command,
        const Json& args
    ) const override {
        return command == "contract.echo" &&
            args.contains("value") &&
            args.at("value").is_string();
    }

    Json Handle(
        const std::string& command,
        const Json& args,
        const KSMPluginContext& context
    ) override {
        if (
            command != "contract.echo" ||
            !context.has_permission("contract.echo")
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The custom plugin was not authorized."
            );
        }
        return Json{{"value", args.at("value")}};
    }
};

class ReplacementFileSystemPlugin final : public KSMPlugin {
public:
    const std::string& Id() const override {
        static const std::string id = "fs";
        return id;
    }

    const std::vector<std::string>& Commands() const override {
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

    const std::vector<std::string>& Permissions() const override {
        static const std::vector<std::string> permissions = {
            "fs.read",
            "fs.write",
            "fs.remove"
        };
        return permissions;
    }

    Json Handle(
        const std::string& command,
        const Json& args,
        const KSMPluginContext&
    ) override {
        (void)args;
        if (command == "fs.exists") {
            return Json{{"exists", true}};
        }
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The replacement filesystem only supports fs.exists."
        );
    }
};

class ReplacementClipboardPlugin final : public KSMPlugin {
public:
    const std::string& Id() const override {
        static const std::string id = "clipboard";
        return id;
    }

    const std::vector<std::string>& Commands() const override {
        static const std::vector<std::string> commands = {
            "clipboard.readText",
            "clipboard.writeText"
        };
        return commands;
    }

    const std::vector<std::string>& Permissions() const override {
        static const std::vector<std::string> permissions = {
            "clipboard.read",
            "clipboard.write"
        };
        return permissions;
    }

    Json Handle(
        const std::string& command,
        const Json&,
        const KSMPluginContext&
    ) override {
        if (command == "clipboard.readText") {
            return Json{{"text", "replacement"}};
        }
        return Json{{"written", true}};
    }
};

class GeneratedCommandHijackPlugin final : public KSMPlugin {
public:
    const std::string& Id() const override {
        static const std::string id = "contract.hijack";
        return id;
    }

    const std::vector<std::string>& Commands() const override {
        static const std::vector<std::string> commands = {"fs.exists"};
        return commands;
    }

    Json Handle(
        const std::string&,
        const Json&,
        const KSMPluginContext&
    ) override {
        return Json{{"exists", false}};
    }
};

class LimitSqlConnection final : public KSMSqlConnection {
public:
    KSMSqlQueryResult Query(
        const std::string& statement,
        const std::vector<KSMSqlValue>&
    ) override {
        if (statement == "rows") {
            return KSMSqlQueryResult{
                {"value"},
                std::vector<std::vector<KSMSqlValue>>(
                    10001,
                    std::vector<KSMSqlValue>{std::int64_t{1}}
                )
            };
        }
        if (statement == "bytes") {
            return KSMSqlQueryResult{
                {"value"},
                {{std::string(16 * 1024 * 1024 + 1, 'x')}}
            };
        }
        return KSMSqlQueryResult{
            {"value"},
            {{std::int64_t{1}}}
        };
    }

    KSMSqlExecuteResult Execute(
        const std::string&,
        const std::vector<KSMSqlValue>&
    ) override {
        return KSMSqlExecuteResult{0, std::nullopt};
    }
};

class LimitSqlDriver final : public KSMSqlDriver {
public:
    std::string Name() const override {
        return "sqlite";
    }

    std::unique_ptr<KSMSqlConnection> Open(
        const std::string&
    ) override {
        return std::make_unique<LimitSqlConnection>();
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
        {"id", "timeout-overflow"},
        {"cmd", "dialog.message"},
        {"args", {{"title", "Kasumi"}, {"message", "wait"}}},
        {"timeoutMs", 600001}
    }.dump());
    const auto overflow = responses.WaitFor(
        [](const Json& response) {
            return IsResponse(response, "timeout-overflow");
        },
        std::chrono::seconds(1)
    );
    if (!overflow) {
        throw std::runtime_error(
            "The host did not reject an overflowing timeout."
        );
    }
    ExpectError(*overflow, "INVALID_ARGS");

    host.OnMessageReceived(Json{
        {"type", "request"},
        {"id", "duplicate-request"},
        {"cmd", "dialog.message"},
        {"args", {{"title", "Kasumi"}, {"message", "wait"}}},
        {"timeoutMs", 5000}
    }.dump());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    host.OnMessageReceived(Json{
        {"type", "request"},
        {"id", "duplicate-request"},
        {"cmd", "ping"},
        {"args", Json::object()}
    }.dump());
    const auto duplicate = responses.WaitFor(
        [](const Json& response) {
            return IsResponse(response, "duplicate-request");
        },
        std::chrono::seconds(1)
    );
    if (!duplicate) {
        throw std::runtime_error(
            "The host did not reject a duplicate request ID."
        );
    }
    ExpectError(*duplicate, "BUSY");
    host.OnMessageReceived(Json{
        {"type", "cancel"},
        {"id", "duplicate-request"}
    }.dump());
    responses.WaitFor(
        [](const Json& response) {
            return IsResponse(response, "duplicate-request");
        },
        std::chrono::seconds(1)
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
        app_info->version != KASUMI_VERSION_STRING ||
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
        "https://app.local/index.html"
    }) {
        if (!KSMWebViewSecurity::IsAllowedOrigin(origin)) {
            throw std::runtime_error(
                std::string("A trusted origin was rejected: ") + origin
            );
        }
    }
    for (const auto& origin : {
        "http://localhost:5173",
        "http://localhost:5173/index.html"
    }) {
        if (KSMWebViewSecurity::IsAllowedOrigin(origin)) {
            throw std::runtime_error(
                std::string("The development origin was accepted in production: ") +
                origin
            );
        }
        if (!KSMWebViewSecurity::IsAllowedOrigin(origin, true)) {
            throw std::runtime_error(
                std::string("The development origin was rejected in dev mode: ") +
                origin
            );
        }
    }
    if (
        KSMWebViewSecurity::IsAllowedNavigation("https://example.com", false) ||
        !KSMWebViewSecurity::IsAllowedNavigation(
            "http://localhost:5173/index.html",
            true
        )
    ) {
        throw std::runtime_error("WebView navigation origin policy is incorrect.");
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

void TestUtilityPlugins(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    auto services = std::make_shared<BlockingHostServices>();
    KSMRouter router(file_system, services);

    const auto directories = Invoke(router, "path.dirs", Json::object());
    ExpectOk(directories);
    if (
        directories.at("data").at("data") != "app://data" ||
        directories.at("data").at("resource") != "app://res"
    ) {
        throw std::runtime_error("path.dirs returned unexpected roots.");
    }
    const auto joined = Invoke(
        router,
        "path.join",
        Json{
            {"base", "app://data"},
            {"segments", Json::array({"notes", "entry.json"})}
        }
    );
    ExpectOk(joined);
    if (joined.at("data").at("path") != "app://data/notes/entry.json") {
        throw std::runtime_error("path.join returned an unexpected path.");
    }
    const auto normalized = Invoke(
        router,
        "path.normalize",
        Json{{"path", "app://data/notes/../entry.json"}}
    );
    ExpectError(normalized, "INVALID_ARGS");
    const auto dirname = Invoke(
        router,
        "path.dirname",
        Json{{"path", "app://data/notes/entry.json"}}
    );
    ExpectOk(dirname);
    if (dirname.at("data").at("path") != "app://data/notes") {
        throw std::runtime_error("path.dirname returned an unexpected path.");
    }
    const auto basename = Invoke(
        router,
        "path.basename",
        Json{
            {"path", "app://data/notes/entry.json"},
            {"suffix", ".json"}
        }
    );
    ExpectOk(basename);
    if (basename.at("data").at("name") != "entry") {
        throw std::runtime_error("path.basename did not remove the suffix.");
    }
    const auto extension = Invoke(
        router,
        "path.extname",
        Json{{"path", "app://data/notes/entry.json"}}
    );
    ExpectOk(extension);
    if (extension.at("data").at("extension") != ".json") {
        throw std::runtime_error("path.extname returned an unexpected extension.");
    }
    const auto absolute = Invoke(
        router,
        "path.isAbsolute",
        Json{{"path", "app://data"}}
    );
    ExpectOk(absolute);
    if (absolute.at("data").at("absolute") != true) {
        throw std::runtime_error("path.isAbsolute rejected a virtual root.");
    }
    ExpectError(
        Invoke(
            router,
            "path.join",
            Json{
                {"base", "app://data"},
                {"segments", Json::array({"../secret"})}
            }
        ),
        "INVALID_ARGS"
    );

    const Json profile{
        {"theme", "light"},
        {"notifications", true},
        {"tabs", Json::array({"home", "work"})}
    };
    ExpectOk(Invoke(
        router,
        "store.set",
        Json{{"key", "profile"}, {"value", profile}}
    ));
    const auto stored = Invoke(
        router,
        "store.get",
        Json{{"key", "profile"}}
    );
    ExpectOk(stored);
    if (
        stored.at("data").at("found") != true ||
        stored.at("data").at("value") != profile
    ) {
        throw std::runtime_error("store.get returned unexpected data.");
    }
    const auto keys = Invoke(router, "store.keys", Json::object());
    ExpectOk(keys);
    if (
        keys.at("data").at("keys").size() != 1 ||
        keys.at("data").at("keys").at(0) != "profile"
    ) {
        throw std::runtime_error("store.keys returned unexpected keys.");
    }
    ExpectOk(Invoke(
        router,
        "store.delete",
        Json{{"key", "profile"}}
    ));
    const auto missing = Invoke(
        router,
        "store.get",
        Json{{"key", "profile"}}
    );
    ExpectOk(missing);
    if (missing.at("data").at("found") != false) {
        throw std::runtime_error("store.get reported a deleted key.");
    }

    if (!file_system->WriteText("app://data/.kasumi/store.json", "{")) {
        throw std::runtime_error("The corrupt store fixture could not be written.");
    }
    ExpectError(
        Invoke(router, "store.get", Json{{"key", "profile"}}),
        "IO_ERROR"
    );
    ExpectOk(Invoke(router, "store.clear", Json::object()));

    const auto operating_system = Invoke(
        router,
        "os.getInfo",
        Json::object()
    );
    ExpectOk(operating_system);
    if (
        operating_system.at("data").at("platform") != "windows" ||
        operating_system.at("data").at("architecture") != "x86_64" ||
        operating_system.at("data").at("timezone") != "Asia/Shanghai"
    ) {
        throw std::runtime_error("os.getInfo returned unexpected data.");
    }
    ExpectOk(Invoke(
        router,
        "log.write",
        Json{
            {"level", "info"},
            {"message", "contract"},
            {"target", "kasumi"}
        }
    ));
    if (
        services->logged_level != "info" ||
        services->logged_target != "kasumi" ||
        services->logged_message != "contract"
    ) {
        throw std::runtime_error("log.write did not reach the host logger.");
    }

    KSMPluginContext denied{
        file_system,
        services,
        [](const std::string&) {
            return false;
        },
        ""
    };
    try {
        KSMStorePlugin plugin;
        plugin.Handle("store.get", Json{{"key", "profile"}}, denied);
        throw std::runtime_error("A store plugin call bypassed permissions.");
    } catch (const KSMHostException& error) {
        if (error.code() != "PERMISSION_DENIED") {
            throw;
        }
    }

    KSM::Init(file_system, services);
    const auto typed_directories = KSM::Path::Dirs();
    const auto typed_joined = KSM::Path::Join(
        "app://data",
        {"typed", "entry.json"}
    );
    const auto typed_basename = KSM::Path::Basename(
        "app://data/typed/entry.json",
        ".json"
    );
    const auto typed_extension = KSM::Path::Extname(
        "app://data/typed/entry.json"
    );
    if (
        !typed_directories ||
        typed_directories->data != "app://data" ||
        typed_joined != "app://data/typed/entry.json" ||
        typed_basename != "entry" ||
        typed_extension != ".json"
    ) {
        KSM::Shutdown();
        throw std::runtime_error("The typed path API returned unexpected data.");
    }
    if (
        !KSM::Store::Set("typed", R"({"enabled":true})")
    ) {
        KSM::Shutdown();
        throw std::runtime_error("The typed store API could not save a value.");
    }
    const auto typed_stored = KSM::Store::Get("typed");
    const auto typed_keys = KSM::Store::Keys();
    if (
        !typed_stored ||
        !typed_stored->found ||
        Json::parse(typed_stored->value_json).at("enabled") != true ||
        !typed_keys ||
        std::find(typed_keys->begin(), typed_keys->end(), "typed") ==
            typed_keys->end()
    ) {
        KSM::Shutdown();
        throw std::runtime_error("The typed store API returned unexpected data.");
    }
    const auto typed_os = KSM::Os::GetInfo();
    if (
        !KSM::Store::Clear() ||
        !typed_os ||
        typed_os->app_version != KASUMI_VERSION_STRING ||
        !KSM::Log::Write(
            KSM::LogLevel::Warn,
            "typed contract",
            "typed"
        ) ||
        services->logged_level != "warn" ||
        services->logged_target != "typed"
    ) {
        KSM::Shutdown();
        throw std::runtime_error("The typed utility API did not reach the host.");
    }
    KSM::Shutdown();
}

void TestSqlPlugin(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    auto services = std::make_shared<BlockingHostServices>();
    services->RegisterSqlDriver(
        std::make_shared<KSMSqliteDriver>(file_system)
    );
    KSMRouter router(file_system, services);

    const auto drivers = Invoke(router, "sql.drivers", Json::object());
    ExpectOk(drivers);
    if (
        drivers.at("data").at("drivers").size() != 1 ||
        drivers.at("data").at("drivers").at(0) != "sqlite"
    ) {
        throw std::runtime_error("sql.drivers did not report SQLite.");
    }

    const auto opened = Invoke(
        router,
        "sql.open",
        Json{{"driver", "sqlite"}, {"url", ":memory:"}}
    );
    ExpectOk(opened);
    const std::string connection_id =
        opened.at("data").at("connectionId").get<std::string>();

    const auto created = Invoke(
        router,
        "sql.execute",
        Json{
            {"connectionId", connection_id},
            {"statement", "CREATE TABLE contract (id INTEGER PRIMARY KEY, name TEXT)"}
        }
    );
    ExpectOk(created);
    if (
        created.at("data").at("rowsAffected") != 0 ||
        !created.at("data").at("lastInsertId").is_null()
    ) {
        throw std::runtime_error("SQLite DDL returned mutation metadata.");
    }
    const auto inserted = Invoke(
        router,
        "sql.execute",
        Json{
            {"connectionId", connection_id},
            {"statement", "INSERT INTO contract (name) VALUES (?)"},
            {"params", Json::array({"kasumi"})}
        }
    );
    ExpectOk(inserted);
    if (
        inserted.at("data").at("rowsAffected") != 1 ||
        inserted.at("data").at("lastInsertId") != 1
    ) {
        throw std::runtime_error("sql.execute returned invalid changes.");
    }

    const auto updated = Invoke(
        router,
        "sql.execute",
        Json{
            {"connectionId", connection_id},
            {"statement", "UPDATE contract SET name = ? WHERE id = ?"},
            {"params", Json::array({"updated", 1})}
        }
    );
    ExpectOk(updated);
    if (
        updated.at("data").at("rowsAffected") != 1 ||
        !updated.at("data").at("lastInsertId").is_null()
    ) {
        throw std::runtime_error("SQLite UPDATE returned an insert ID.");
    }

    const auto queried = Invoke(
        router,
        "sql.query",
        Json{
            {"connectionId", connection_id},
            {"statement", "SELECT id, name FROM contract WHERE id = ?"},
            {"params", Json::array({1})}
        }
    );
    ExpectOk(queried);
    if (
        queried.at("data").at("columns").at(1) != "name" ||
        queried.at("data").at("rows").at(0).at("name") != "updated"
    ) {
        throw std::runtime_error("sql.query returned invalid rows.");
    }

    ExpectOk(Invoke(
        router,
        "sql.close",
        Json{{"connectionId", connection_id}}
    ));
    ExpectError(
        Invoke(
            router,
            "sql.open",
            Json{{"driver", "mysql"}, {"url", "mysql://localhost/db"}}
        ),
        "NOT_SUPPORTED"
    );
    ExpectError(
        Invoke(
            router,
            "sql.open",
            Json{{"driver", "sqlite"}, {"url", "app://picked/db.sqlite"}}
        ),
        "PERMISSION_DENIED"
    );
}

void TestSqlResourceLimits(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    auto services = std::make_shared<BlockingHostServices>();
    services->RegisterSqlDriver(std::make_shared<LimitSqlDriver>());
    KSMRouter router(file_system, services);

    std::string first_connection;
    for (std::size_t index = 0; index < 32; ++index) {
        const auto opened = Invoke(
            router,
            "sql.open",
            Json{{"driver", "sqlite"}, {"url", ":memory:"}}
        );
        ExpectOk(opened);
        if (index == 0) {
            first_connection =
                opened.at("data").at("connectionId").get<std::string>();
        }
    }
    ExpectError(
        Invoke(
            router,
            "sql.open",
            Json{{"driver", "sqlite"}, {"url", ":memory:"}}
        ),
        "RESOURCE_LIMIT"
    );

    ExpectError(
        Invoke(
            router,
            "sql.query",
            Json{
                {"connectionId", first_connection},
                {"statement", "rows"}
            }
        ),
        "RESOURCE_LIMIT"
    );
    ExpectError(
        Invoke(
            router,
            "sql.query",
            Json{
                {"connectionId", first_connection},
                {"statement", "bytes"}
            }
        ),
        "RESOURCE_LIMIT"
    );
    ExpectOk(Invoke(
        router,
        "sql.close",
        Json{{"connectionId", first_connection}}
    ));
    ExpectOk(Invoke(
        router,
        "sql.open",
        Json{{"driver", "sqlite"}, {"url", ":memory:"}}
    ));
}

void TestCustomPlugin(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    KSMRouter router(
        file_system,
        nullptr,
        {},
        {std::make_shared<ContractEchoPlugin>()}
    );
    const auto echoed = Invoke(
        router,
        "contract.echo",
        Json{{"value", "plugin"}}
    );
    ExpectOk(echoed);
    if (echoed.at("data").at("value") != "plugin") {
        throw std::runtime_error("The custom plugin returned invalid data.");
    }
    const auto capabilities = Invoke(
        router,
        "capabilities.query",
        Json::object()
    );
    ExpectOk(capabilities);
    if (
        std::find(
            capabilities.at("data").at("plugins").begin(),
            capabilities.at("data").at("plugins").end(),
            "contract.echo"
        ) == capabilities.at("data").at("plugins").end()
    ) {
        throw std::runtime_error(
            "capabilities.query omitted the custom plugin."
        );
    }
}

void TestPluginRules(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    KSMRouter replacement(
        file_system,
        nullptr,
        {},
        {std::make_shared<ReplacementFileSystemPlugin>()}
    );
    const auto exists = Invoke(
        replacement,
        "fs.exists",
        Json{{"path", "app://data/plugin.txt"}}
    );
    ExpectOk(exists);
    if (exists.at("data").at("exists") != true) {
        throw std::runtime_error("The replacement filesystem was not used.");
    }

    bool hijack_rejected = false;
    try {
        KSMRouter invalid(
            file_system,
            nullptr,
            {},
            {std::make_shared<GeneratedCommandHijackPlugin>()}
        );
        (void)invalid;
    } catch (const std::invalid_argument&) {
        hijack_rejected = true;
    }
    if (!hijack_rejected) {
        throw std::runtime_error(
            "A custom plugin hijacked a generated command."
        );
    }

    KSMRouter clipboard(
        file_system,
        nullptr,
        {},
        {std::make_shared<ReplacementClipboardPlugin>()}
    );
    const auto clipboard_text = Invoke(
        clipboard,
        "clipboard.readText",
        Json::object()
    );
    ExpectOk(clipboard_text);
    if (clipboard_text.at("data").at("text") != "replacement") {
        throw std::runtime_error(
            "The replacement clipboard plugin was not used."
        );
    }
}

void TestCommandAllowlist(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    const std::vector<std::string> allowed{
        "ping",
        "capabilities.query"
    };
    KSMRouter router(file_system, nullptr, allowed);

    const auto capabilities = Invoke(
        router,
        "capabilities.query",
        Json::object()
    );
    ExpectOk(capabilities);
    const auto& commands = capabilities.at("data").at("commands");
    if (
        commands.size() != allowed.size() ||
        std::find(commands.begin(), commands.end(), "ping") == commands.end() ||
        std::find(
            commands.begin(),
            commands.end(),
            "capabilities.query"
        ) == commands.end()
    ) {
        throw std::runtime_error(
            "capabilities.query ignored the configured command allowlist."
        );
    }
    ExpectError(
        Invoke(
            router,
            "fs.writeText",
            Json{{"path", "app://data/blocked.txt"}, {"content", "blocked"}}
        ),
        "NOT_SUPPORTED"
    );

    KSM::Init(file_system, nullptr, allowed);
    if (KSM::Get().commands != allowed) {
        throw std::runtime_error(
            "KSM::Get ignored the configured command allowlist."
        );
    }
    if (KSM::Fs::WriteText("app://data/blocked.txt", "blocked")) {
        throw std::runtime_error(
            "KSM::Fs bypassed the configured command allowlist."
        );
    }
    KSM::Shutdown();
}

void TestWindowsHostVersion() {
    KSMWindowsHostServices services(
        nullptr,
        {},
        nullptr,
        "2.3.4-beta.1"
    );
    if (services.AppVersion() != "2.3.4-beta.1") {
        throw std::runtime_error(
            "KSMWindowsHostServices did not retain the configured version."
        );
    }
}

void TestDuplicateDownloadRequest(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    KSMWindowsHostServices services(nullptr, {}, file_system);
    std::exception_ptr first_error;
    std::thread download([&services, &first_error] {
        try {
            (void)services.Download(
                "https://203.0.113.1/kasumi.bin",
                "app://data/duplicate-download.bin",
                false,
                "duplicate-download"
            );
        } catch (...) {
            first_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bool duplicate_rejected = false;
    try {
        (void)services.Download(
            "https://203.0.113.1/kasumi.bin",
            "app://data/duplicate-download.bin",
            false,
            "duplicate-download"
        );
    } catch (const KSMHostException& error) {
        duplicate_rejected = error.code() == "BUSY";
    }
    services.CancelRequest("duplicate-download");
    download.join();
    if (!duplicate_rejected) {
        throw std::runtime_error(
            "The Windows host did not reject a duplicate download ID."
        );
    }
    (void)first_error;
}

void TestConcurrentStoreWrites(
    const std::shared_ptr<KSMFileSystem>& file_system
) {
    KSMStorePlugin first_plugin;
    KSMStorePlugin second_plugin;
    const KSMPluginContext context{
        file_system,
        nullptr,
        [](const std::string& permission) {
            return permission == "store";
        },
        ""
    };
    std::exception_ptr first_error;
    std::exception_ptr second_error;
    std::thread first([&] {
        try {
            for (int index = 0; index < 20; ++index) {
                (void)first_plugin.Handle(
                    "store.set",
                    Json{
                        {"key", "first-" + std::to_string(index)},
                        {"value", index}
                    },
                    context
                );
            }
        } catch (...) {
            first_error = std::current_exception();
        }
    });
    std::thread second([&] {
        try {
            for (int index = 0; index < 20; ++index) {
                (void)second_plugin.Handle(
                    "store.set",
                    Json{
                        {"key", "second-" + std::to_string(index)},
                        {"value", index}
                    },
                    context
                );
            }
        } catch (...) {
            second_error = std::current_exception();
        }
    });
    first.join();
    second.join();
    if (first_error || second_error) {
        throw std::runtime_error(
            "Concurrent store writes returned an error."
        );
    }
    const auto keys = first_plugin.Handle(
        "store.keys",
        Json::object(),
        context
    );
    if (keys.at("keys").size() != 40) {
        throw std::runtime_error(
            "Concurrent store writes lost an update."
        );
    }
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
        if (
            std::find(
                capabilities.at("data").at("commands").begin(),
                capabilities.at("data").at("commands").end(),
                "device.getInfo"
            ) != capabilities.at("data").at("commands").end()
        ) {
            throw std::runtime_error(
                "capabilities.query advertised an Android-only plugin command."
            );
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
        if (file_system->Remove("app://data/contract-dir")) {
            throw std::runtime_error(
                "fs.remove removed a non-empty directory."
            );
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
        TestWindowsHostVersion();
        TestDuplicateDownloadRequest(file_system);
        TestWebViewSecurity();
        TestUrlPolicy();
        TestNewCommands(file_system);
        TestUtilityPlugins(file_system);
        TestConcurrentStoreWrites(file_system);
        TestSqlPlugin(file_system);
        TestSqlResourceLimits(file_system);
        TestCustomPlugin(file_system);
        TestPluginRules(file_system);
        TestCommandAllowlist(file_system);
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
