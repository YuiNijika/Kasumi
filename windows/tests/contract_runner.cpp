#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "../src/bridge/ksm_router.h"
#include "../src/commands/ksm_filesystem.h"

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

}  // namespace

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("kasumi-contract-" + std::to_string(suffix));
    auto file_system = std::make_shared<KSMFileSystem>(
        root / "data",
        root / "cache",
        root / "tmp"
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
        ExpectError(
            Invoke(router, "event.subscribe", Json{{"event", "unknown.event"}}),
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
        if (read.at("data").at("text") != "kasumi") {
            throw std::runtime_error("fs.readText returned unexpected content.");
        }
        ExpectOk(Invoke(
            router,
            "fs.remove",
            Json{{"path", "app://data/contract.txt"}}
        ));

        const std::string large_content(70 * 1024, 'x');
        ExpectError(
            Invoke(
                router,
                "fs.writeText",
                Json{{"path", "app://data/large.txt"}, {"content", large_content}}
            ),
            "INVALID_ARGS"
        );
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
