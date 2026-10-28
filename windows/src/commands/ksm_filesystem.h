#pragma once

#include <filesystem>
#include <optional>
#include <string>

class KSMFileSystem {
public:
    static constexpr std::size_t kMaxTextBytes = 16 * 1024 * 1024;

    KSMFileSystem(
        std::filesystem::path data_root,
        std::filesystem::path cache_root,
        std::filesystem::path temp_root
    );

    std::optional<std::string> ReadText(const std::string& path) const;
    bool WriteText(const std::string& path, const std::string& content) const;
    bool Exists(const std::string& path) const;
    bool Remove(const std::string& path) const;

private:
    std::optional<std::filesystem::path> Resolve(const std::string& path) const;

    std::filesystem::path data_root_;
    std::filesystem::path cache_root_;
    std::filesystem::path temp_root_;
};
