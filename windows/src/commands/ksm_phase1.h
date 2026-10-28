#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

class KSMFileSystem;
class KSMHostServices;

struct KSM {
    enum class Platform {
        Windows,
    };

    struct Capabilities {
        Platform platform;
        std::vector<std::string> commands;
        std::vector<std::string> permissions;
    };

    struct PingResult {
        bool pong;
    };

    struct Fs {
        explicit Fs(std::shared_ptr<KSMFileSystem> file_system);

        std::optional<std::string> ReadText(const std::string& path) const;
        bool WriteText(const std::string& path, const std::string& content) const;
        bool Exists(const std::string& path) const;
        bool Remove(const std::string& path) const;

    private:
        std::shared_ptr<KSMFileSystem> file_system_;
    };

    struct Dialog {
        explicit Dialog(std::shared_ptr<KSMHostServices> host_services);

        bool Message(const std::string& title, const std::string& message) const;
        bool Confirm(const std::string& title, const std::string& message) const;
        std::optional<std::string> OpenFile(
            const std::vector<std::string>& filters
        ) const;
        std::optional<std::string> SaveFile(
            const std::string& suggested_name
        ) const;

    private:
        std::shared_ptr<KSMHostServices> host_services_;
    };

    static const Capabilities& Get();

    struct Phase1 {
        static PingResult Ping();
        static Capabilities QueryCapabilities();
    };
};
