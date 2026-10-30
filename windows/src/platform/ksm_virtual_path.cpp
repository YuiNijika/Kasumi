#include "ksm_virtual_path.h"

namespace {

constexpr char kPickedPrefix[] = "app://picked/";
constexpr char kResourcePrefix[] = "app://res/";
constexpr char kAssetPrefix[] = "app://assets/";

bool IsSafeRelativePath(const std::string& relative) {
    return !relative.empty() &&
        relative.find('\0') == std::string::npos &&
        relative.find("..") == std::string::npos &&
        relative.find('~') == std::string::npos &&
        relative.find(':') == std::string::npos &&
        relative.find('\\') == std::string::npos &&
        !relative.starts_with('/');
}

bool HasForbiddenPrefix(const std::string& path) {
    return path.find('\0') != std::string::npos ||
        path.find("..") != std::string::npos ||
        path.find('~') != std::string::npos ||
        path.starts_with('/') ||
        path.starts_with('\\') ||
        (path.size() > 1 && path[1] == ':');
}

bool IsVirtualRoot(const std::string& path) {
    return path == "app://data" ||
        path == "app://cache" ||
        path == "app://tmp" ||
        path == "app://picked" ||
        path == "app://res" ||
        path == "app://assets";
}

std::string StripTrailingSlashes(const std::string& path) {
    std::string result = path;
    while (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

bool HasRepeatedTrailingSlash(const std::string& path) {
    return path.size() >= 2 &&
        path.back() == '/' &&
        path[path.size() - 2] == '/';
}

}

bool KSMVirtualPath::IsSafe(const std::string& path) {
    if (HasForbiddenPrefix(path)) {
        return false;
    }
    if (
        path == "app://data" ||
        path == "app://cache" ||
        path == "app://tmp" ||
        path == "app://picked" ||
        path == "app://res" ||
        path == "app://assets"
    ) {
        return false;
    }
    const std::string prefixes[] = {
        "app://data/",
        "app://cache/",
        "app://tmp/",
        "app://picked/",
        kResourcePrefix,
        kAssetPrefix
    };
    for (const auto& prefix : prefixes) {
        if (path.starts_with(prefix)) {
            const std::string relative = path.substr(prefix.size());
            if (path.starts_with(kPickedPrefix)) {
                return IsSafeRelativePath(relative) &&
                    relative.find('/') == std::string::npos;
            }
            return IsSafeRelativePath(relative);
        }
    }
    return false;
}

bool KSMVirtualPath::IsSafeDirectory(const std::string& path) {
    if (
        HasForbiddenPrefix(path) ||
        HasRepeatedTrailingSlash(path) ||
        path == "app://picked" ||
        path.starts_with(kPickedPrefix)
    ) {
        return false;
    }
    const std::string normalized = NormalizeDirectory(path);
    if (IsVirtualRoot(normalized)) {
        return true;
    }
    return IsSafe(normalized);
}

bool KSMVirtualPath::IsSafeStat(const std::string& path) {
    if (HasForbiddenPrefix(path) || HasRepeatedTrailingSlash(path)) {
        return false;
    }
    const std::string normalized = NormalizeDirectory(path);
    if (normalized == "app://picked") {
        return false;
    }
    return IsVirtualRoot(normalized) || IsSafe(normalized);
}

bool KSMVirtualPath::IsPicked(const std::string& path) {
    if (!IsSafe(path) || !path.starts_with(kPickedPrefix)) {
        return false;
    }
    const std::string token = path.substr(sizeof(kPickedPrefix) - 1);
    return !token.empty() && token.find('/') == std::string::npos;
}

bool KSMVirtualPath::IsReadOnly(const std::string& path) {
    return path.starts_with(kResourcePrefix) || path.starts_with(kAssetPrefix);
}

std::string KSMVirtualPath::PickedPath(const std::string& token) {
    return std::string(kPickedPrefix) + token;
}

std::string KSMVirtualPath::NormalizeDirectory(const std::string& path) {
    return StripTrailingSlashes(path);
}

namespace {

std::optional<std::filesystem::path> ResolveUnder(
    const std::string& path,
    const std::string& prefix,
    const std::filesystem::path& root
) {
    if (root.empty() || !path.starts_with(prefix)) {
        return std::nullopt;
    }
    const std::string relative = path.substr(prefix.size());
    if (
        !IsSafeRelativePath(relative)
    ) {
        return std::nullopt;
    }
    return root / relative;
}

std::optional<std::filesystem::path> ResolveDirectoryUnder(
    const std::string& path,
    const std::string& prefix,
    const std::filesystem::path& root
) {
    if (root.empty()) {
        return std::nullopt;
    }
    const std::string normalized = StripTrailingSlashes(path);
    const std::string root_path = prefix.substr(0, prefix.size() - 1);
    if (normalized == root_path) {
        return root;
    }
    if (!normalized.starts_with(prefix)) {
        return std::nullopt;
    }
    const std::string relative = normalized.substr(prefix.size());
    if (!IsSafeRelativePath(relative)) {
        return std::nullopt;
    }
    return root / relative;
}

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
        if (const auto resolved = ResolveUnder(path, prefix, root)) {
            return resolved;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> KSMVirtualPath::ResolveWritableDirectory(
    const std::string& path,
    const std::filesystem::path& data_root,
    const std::filesystem::path& cache_root,
    const std::filesystem::path& temp_root
) {
    if (!IsSafeDirectory(path)) {
        return std::nullopt;
    }

    const std::pair<std::string, std::filesystem::path> roots[] = {
        {"app://data/", data_root},
        {"app://cache/", cache_root},
        {"app://tmp/", temp_root}
    };
    for (const auto& [prefix, root] : roots) {
        if (const auto resolved = ResolveDirectoryUnder(path, prefix, root)) {
            return resolved;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> KSMVirtualPath::ResolveReadable(
    const std::string& path,
    const std::filesystem::path& data_root,
    const std::filesystem::path& cache_root,
    const std::filesystem::path& temp_root,
    const std::filesystem::path& resource_root,
    const std::filesystem::path& asset_root
) {
    if (!IsSafe(path)) {
        return std::nullopt;
    }
    if (const auto resolved = ResolveWritable(
        path,
        data_root,
        cache_root,
        temp_root
    )) {
        return resolved;
    }
    if (const auto resolved = ResolveUnder(
        path,
        kResourcePrefix,
        resource_root
    )) {
        return resolved;
    }
    return ResolveUnder(path, kAssetPrefix, asset_root);
}

std::optional<std::filesystem::path> KSMVirtualPath::ResolveReadableDirectory(
    const std::string& path,
    const std::filesystem::path& data_root,
    const std::filesystem::path& cache_root,
    const std::filesystem::path& temp_root,
    const std::filesystem::path& resource_root,
    const std::filesystem::path& asset_root
) {
    if (!IsSafeDirectory(path)) {
        return std::nullopt;
    }
    if (const auto resolved = ResolveWritableDirectory(
        path,
        data_root,
        cache_root,
        temp_root
    )) {
        return resolved;
    }
    if (const auto resolved = ResolveDirectoryUnder(
        path,
        kResourcePrefix,
        resource_root
    )) {
        return resolved;
    }
    return ResolveDirectoryUnder(path, kAssetPrefix, asset_root);
}
