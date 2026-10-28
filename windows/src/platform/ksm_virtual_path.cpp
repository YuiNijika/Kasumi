#include "ksm_virtual_path.h"

namespace {

constexpr char kPickedPrefix[] = "app://picked/";

bool HasForbiddenPrefix(const std::string& path) {
    return path.find('\0') != std::string::npos ||
        path.find("..") != std::string::npos ||
        path.find('~') != std::string::npos ||
        path.starts_with('/') ||
        path.starts_with('\\') ||
        (path.size() > 1 && path[1] == ':');
}

}

bool KSMVirtualPath::IsSafe(const std::string& path) {
    return !HasForbiddenPrefix(path);
}

bool KSMVirtualPath::IsPicked(const std::string& path) {
    if (!IsSafe(path) || !path.starts_with(kPickedPrefix)) {
        return false;
    }
    const std::string token = path.substr(sizeof(kPickedPrefix) - 1);
    return !token.empty() && token.find('/') == std::string::npos;
}

std::string KSMVirtualPath::PickedPath(const std::string& token) {
    return std::string(kPickedPrefix) + token;
}

std::optional<std::filesystem::path> KSMVirtualPath::ResolveWritable(
    const std::string& path,
    const std::filesystem::path& data_root,
    const std::filesystem::path& cache_root,
    const std::filesystem::path& temp_root
) {
    if (!IsSafe(path)) {
        return std::nullopt;
    }

    const std::pair<std::string, std::filesystem::path> roots[] = {
        {"app://data/", data_root},
        {"app://cache/", cache_root},
        {"app://tmp/", temp_root}
    };
    for (const auto& [prefix, root] : roots) {
        if (path.starts_with(prefix)) {
            const std::string relative = path.substr(prefix.size());
            if (
                relative.empty() ||
                relative.starts_with('/') ||
                relative.starts_with('\\')
            ) {
                return std::nullopt;
            }
            return root / relative;
        }
    }
    return std::nullopt;
}
