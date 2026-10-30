#pragma once

#include <filesystem>
#include <atomic>
#include <optional>
#include <string>
#include <vector>

#include "../platform/ksm_file_metadata.h"

class KSMFileSystem {
public:
    static constexpr std::size_t kMaxTextBytes = 16 * 1024 * 1024;

    KSMFileSystem(
        std::filesystem::path data_root,
        std::filesystem::path cache_root,
        std::filesystem::path temp_root,
        std::filesystem::path resource_root = {},
        std::filesystem::path asset_root = {}
    );

    std::optional<std::string> ReadText(const std::string& path) const;
    bool WriteText(const std::string& path, const std::string& content) const;
    bool Exists(const std::string& path) const;
    bool Remove(const std::string& path) const;
    std::optional<std::vector<KSMDirectoryEntry>> ReadDir(
        const std::string& path
    ) const;
    std::optional<bool> MakeDirectory(
        const std::string& path,
        bool recursive
    ) const;
    std::optional<KSMFileMetadata> Stat(const std::string& path) const;
    std::optional<std::string> CreateTemporaryText(
        const std::string& content
    ) const;

private:
    std::optional<std::filesystem::path> ResolveReadable(
        const std::string& path
    ) const;
    std::optional<std::filesystem::path> ResolveWritable(
        const std::string& path
    ) const;
    std::optional<std::filesystem::path> ResolveReadableDirectory(
        const std::string& path
    ) const;
    std::optional<std::filesystem::path> ResolveWritableDirectory(
        const std::string& path
    ) const;

    std::filesystem::path data_root_;
    std::filesystem::path cache_root_;
    std::filesystem::path temp_root_;
    std::filesystem::path resource_root_;
    std::filesystem::path asset_root_;
    mutable std::atomic<std::uint64_t> next_temp_id_{1};
};
