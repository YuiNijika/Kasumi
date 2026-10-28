#include "ksm_filesystem.h"

#include <fstream>
#include <iterator>
#include <utility>

#include "../platform/ksm_virtual_path.h"

KSMFileSystem::KSMFileSystem(
    std::filesystem::path data_root,
    std::filesystem::path cache_root,
    std::filesystem::path temp_root
)
    : data_root_(std::move(data_root)),
      cache_root_(std::move(cache_root)),
      temp_root_(std::move(temp_root)) {
}

std::optional<std::filesystem::path> KSMFileSystem::Resolve(
    const std::string& path
) const {
    return KSMVirtualPath::ResolveWritable(
        path,
        data_root_,
        cache_root_,
        temp_root_
    );
}

std::optional<std::string> KSMFileSystem::ReadText(
    const std::string& path
) const {
    const auto resolved = Resolve(path);
    if (!resolved || !std::filesystem::is_regular_file(*resolved)) {
        return std::nullopt;
    }
    std::error_code size_error;
    const auto size = std::filesystem::file_size(*resolved, size_error);
    if (size_error || size > kMaxTextBytes) {
        return std::nullopt;
    }
    std::ifstream input(*resolved, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
}

bool KSMFileSystem::WriteText(
    const std::string& path,
    const std::string& content
) const {
    if (content.size() > kMaxTextBytes) {
        return false;
    }
    const auto resolved = Resolve(path);
    if (!resolved) {
        return false;
    }
    std::filesystem::create_directories(resolved->parent_path());
    std::ofstream output(*resolved, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output << content;
    return output.good();
}

bool KSMFileSystem::Exists(const std::string& path) const {
    const auto resolved = Resolve(path);
    return resolved.has_value() && std::filesystem::exists(*resolved);
}

bool KSMFileSystem::Remove(const std::string& path) const {
    const auto resolved = Resolve(path);
    return resolved.has_value() && std::filesystem::remove(*resolved);
}
