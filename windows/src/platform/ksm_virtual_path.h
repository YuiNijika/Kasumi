#pragma once

#include <filesystem>
#include <optional>
#include <string>

class KSMVirtualPath {
public:
    static bool IsSafe(const std::string& path);
    static bool IsPicked(const std::string& path);
    static std::string PickedPath(const std::string& token);
    static std::optional<std::filesystem::path> ResolveWritable(
        const std::string& path,
        const std::filesystem::path& data_root,
        const std::filesystem::path& cache_root,
        const std::filesystem::path& temp_root
    );
};
