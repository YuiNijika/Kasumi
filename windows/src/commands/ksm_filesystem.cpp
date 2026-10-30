#include "ksm_filesystem.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "../platform/ksm_virtual_path.h"

namespace {

std::string ToUtf8(const std::filesystem::path& path) {
    const std::u8string value = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(value.data()),
        value.size()
    );
}

std::optional<std::int64_t> ModifiedAt(
    const std::filesystem::path& path
) {
    std::error_code error;
    const auto timestamp = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    const auto system_timestamp =
        std::chrono::clock_cast<std::chrono::system_clock>(timestamp);
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        system_timestamp.time_since_epoch()
    ).count();
}

std::optional<KSMFileMetadata> ReadMetadata(
    const std::filesystem::path& path
) {
    std::error_code error;
    const auto link_status = std::filesystem::symlink_status(path, error);
    if (error || link_status.type() == std::filesystem::file_type::symlink) {
        return std::nullopt;
    }
    const auto status = std::filesystem::status(path, error);
    if (error || !std::filesystem::exists(status)) {
        return std::nullopt;
    }
    if (std::filesystem::is_directory(status)) {
        return KSMFileMetadata{
            KSMFileKind::Directory,
            0,
            ModifiedAt(path)
        };
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::nullopt;
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return std::nullopt;
    }
    return KSMFileMetadata{
        KSMFileKind::File,
        size,
        ModifiedAt(path)
    };
}

std::string VirtualChildPath(
    const std::string& parent,
    const std::string& name
) {
    return KSMVirtualPath::NormalizeDirectory(parent) + "/" + name;
}

}

KSMFileSystem::KSMFileSystem(
    std::filesystem::path data_root,
    std::filesystem::path cache_root,
    std::filesystem::path temp_root,
    std::filesystem::path resource_root,
    std::filesystem::path asset_root
)
    : data_root_(std::move(data_root)),
      cache_root_(std::move(cache_root)),
      temp_root_(std::move(temp_root)),
      resource_root_(std::move(resource_root)),
      asset_root_(std::move(asset_root)) {
}

std::optional<std::filesystem::path> KSMFileSystem::ResolveReadable(
    const std::string& path
) const {
    return KSMVirtualPath::ResolveReadable(
        path,
        data_root_,
        cache_root_,
        temp_root_,
        resource_root_,
        asset_root_
    );
}

std::optional<std::filesystem::path> KSMFileSystem::ResolveWritable(
    const std::string& path
) const {
    return KSMVirtualPath::ResolveWritable(
        path,
        data_root_,
        cache_root_,
        temp_root_
    );
}

std::optional<std::filesystem::path> KSMFileSystem::ResolveReadableDirectory(
    const std::string& path
) const {
    return KSMVirtualPath::ResolveReadableDirectory(
        path,
        data_root_,
        cache_root_,
        temp_root_,
        resource_root_,
        asset_root_
    );
}

std::optional<std::filesystem::path> KSMFileSystem::ResolveWritableDirectory(
    const std::string& path
) const {
    return KSMVirtualPath::ResolveWritableDirectory(
        path,
        data_root_,
        cache_root_,
        temp_root_
    );
}

std::optional<std::string> KSMFileSystem::ReadText(
    const std::string& path
) const {
    const auto resolved = ResolveReadable(path);
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
    const auto resolved = ResolveWritable(path);
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
    const auto resolved = ResolveReadable(path);
    return resolved.has_value() && std::filesystem::exists(*resolved);
}

bool KSMFileSystem::Remove(const std::string& path) const {
    const auto resolved = ResolveWritable(path);
    return resolved.has_value() && std::filesystem::remove(*resolved);
}

std::optional<std::vector<KSMDirectoryEntry>> KSMFileSystem::ReadDir(
    const std::string& path
) const {
    const auto resolved = ResolveReadableDirectory(path);
    if (!resolved) {
        return std::nullopt;
    }
    std::error_code error;
    const auto link_status = std::filesystem::symlink_status(*resolved, error);
    if (
        error ||
        link_status.type() == std::filesystem::file_type::symlink ||
        !std::filesystem::is_directory(link_status)
    ) {
        return std::nullopt;
    }

    std::vector<KSMDirectoryEntry> entries;
    std::filesystem::directory_iterator iterator(
        *resolved,
        std::filesystem::directory_options::skip_permission_denied,
        error
    );
    if (error) {
        return std::nullopt;
    }
    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const auto& entry = *iterator;
        const auto metadata = ReadMetadata(entry.path());
        if (metadata) {
            const std::string name = ToUtf8(entry.path().filename());
            if (!name.empty()) {
                entries.push_back(KSMDirectoryEntry{
                    name,
                    VirtualChildPath(path, name),
                    *metadata
                });
            }
        }
        iterator.increment(error);
        if (error) {
            return std::nullopt;
        }
    }
    std::sort(
        entries.begin(),
        entries.end(),
        [](const KSMDirectoryEntry& left, const KSMDirectoryEntry& right) {
            return left.name < right.name;
        }
    );
    return entries;
}

std::optional<bool> KSMFileSystem::MakeDirectory(
    const std::string& path,
    bool recursive
) const {
    const auto resolved = ResolveWritableDirectory(path);
    if (!resolved) {
        return std::nullopt;
    }
    std::error_code error;
    const auto status = std::filesystem::symlink_status(*resolved, error);
    if (!error && std::filesystem::exists(status)) {
        if (
            status.type() == std::filesystem::file_type::symlink ||
            !std::filesystem::is_directory(status)
        ) {
            return std::nullopt;
        }
        return false;
    }
    error.clear();
    const bool created = recursive
        ? std::filesystem::create_directories(*resolved, error)
        : std::filesystem::create_directory(*resolved, error);
    if (error) {
        return std::nullopt;
    }
    if (created) {
        return true;
    }
    const auto final_status = std::filesystem::symlink_status(*resolved, error);
    if (
        !error &&
        std::filesystem::is_directory(final_status) &&
        final_status.type() != std::filesystem::file_type::symlink
    ) {
        return false;
    }
    return std::nullopt;
}

std::optional<KSMFileMetadata> KSMFileSystem::Stat(
    const std::string& path
) const {
    auto resolved = ResolveReadableDirectory(path);
    if (!resolved) {
        resolved = ResolveReadable(path);
    }
    if (!resolved) {
        return std::nullopt;
    }
    return ReadMetadata(*resolved);
}

std::optional<std::string> KSMFileSystem::CreateTemporaryText(
    const std::string& content
) const {
    const std::string virtual_path = "app://tmp/read-" +
        std::to_string(next_temp_id_.fetch_add(1)) + ".txt";
    if (!WriteText(virtual_path, content)) {
        return std::nullopt;
    }
    return virtual_path;
}
