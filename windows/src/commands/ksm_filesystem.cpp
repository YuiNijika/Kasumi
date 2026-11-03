#include "ksm_filesystem.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <windows.h>

#include "../platform/ksm_virtual_path.h"

namespace {

std::string ToUtf8(const std::filesystem::path &path) {
  const std::u8string value = path.u8string();
  return std::string(reinterpret_cast<const char *>(value.data()),
                     value.size());
}

std::optional<std::int64_t> ModifiedAt(const std::filesystem::path &path) {
  std::error_code error;
  const auto timestamp = std::filesystem::last_write_time(path, error);
  if (error) {
    return std::nullopt;
  }
  const auto system_timestamp =
      std::chrono::clock_cast<std::chrono::system_clock>(timestamp);
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             system_timestamp.time_since_epoch())
      .count();
}

std::optional<KSMFileMetadata> ReadMetadata(const std::filesystem::path &path) {
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
    return KSMFileMetadata{KSMFileKind::Directory, 0, ModifiedAt(path)};
  }
  if (!std::filesystem::is_regular_file(status)) {
    return std::nullopt;
  }
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    return std::nullopt;
  }
  return KSMFileMetadata{KSMFileKind::File, size, ModifiedAt(path)};
}

std::string VirtualChildPath(const std::string &parent,
                             const std::string &name) {
  return KSMVirtualPath::NormalizeDirectory(parent) + "/" + name;
}

bool IsWithin(const std::filesystem::path &candidate,
              const std::filesystem::path &root) {
  const auto normalize = [](std::u8string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](char8_t ch) {
      if (ch >= u8'A' && ch <= u8'Z') {
        return static_cast<char8_t>(ch - u8'A' + u8'a');
      }
      return ch;
    });
    return value;
  };
  const auto candidate_text = normalize(candidate.lexically_normal().generic_u8string());
  const auto root_text = normalize(root.lexically_normal().generic_u8string());
  if (candidate_text.size() < root_text.size()) {
    return false;
  }
  if (candidate_text.compare(0, root_text.size(), root_text) != 0) {
    return false;
  }
  return candidate_text.size() == root_text.size() ||
         candidate_text[root_text.size()] == u8'/';
}

std::optional<std::filesystem::path> ValidateContainedTarget(
    const std::filesystem::path &root, const std::filesystem::path &target,
    bool require_existing) {
  std::error_code error;
  const auto canonical_root = std::filesystem::weakly_canonical(root, error);
  if (error) {
    return std::nullopt;
  }
  const auto link_status = std::filesystem::symlink_status(target, error);
  if (!error && link_status.type() == std::filesystem::file_type::symlink) {
    return std::nullopt;
  }
  if (require_existing) {
    const auto canonical_target =
        std::filesystem::canonical(target, error);
    if (error || !IsWithin(canonical_target, canonical_root)) {
      return std::nullopt;
    }
    return canonical_target;
  }
  const auto canonical_parent =
      std::filesystem::weakly_canonical(target.parent_path(), error);
  if (error || !IsWithin(canonical_parent, canonical_root)) {
    return std::nullopt;
  }
  return target;
}

} // namespace

KSMFileSystem::KSMFileSystem(
    std::filesystem::path data_root, std::filesystem::path cache_root,
    std::filesystem::path temp_root, std::filesystem::path resource_root,
    std::filesystem::path asset_root,
    ReadOnlyResourceProvider read_only_resource_provider,
    ReadOnlyDirectoryProvider read_only_directory_provider)
    : data_root_(std::move(data_root)), cache_root_(std::move(cache_root)),
      temp_root_(std::move(temp_root)),
      resource_root_(std::move(resource_root)),
      asset_root_(std::move(asset_root)),
      read_only_resource_provider_(std::move(read_only_resource_provider)),
      read_only_directory_provider_(std::move(read_only_directory_provider)) {
  std::error_code error;
  std::filesystem::create_directories(data_root_, error);
  error.clear();
  std::filesystem::create_directories(cache_root_, error);
  error.clear();
  std::filesystem::create_directories(temp_root_, error);
}

std::optional<std::filesystem::path>
KSMFileSystem::ResolveReadable(const std::string &path) const {
  const auto resolved = KSMVirtualPath::ResolveReadable(
      path, data_root_, cache_root_, temp_root_, resource_root_, asset_root_);
  if (!resolved) {
    return std::nullopt;
  }
  const std::filesystem::path *root = nullptr;
  if (path.starts_with("app://data/")) {
    root = &data_root_;
  } else if (path.starts_with("app://cache/")) {
    root = &cache_root_;
  } else if (path.starts_with("app://tmp/")) {
    root = &temp_root_;
  } else if (path.starts_with("app://res/")) {
    root = &resource_root_;
  } else if (path.starts_with("app://assets/")) {
    root = &asset_root_;
  }
  return root ? ValidateContainedTarget(*root, *resolved, true) : std::nullopt;
}

std::optional<std::filesystem::path>
KSMFileSystem::ResolveWritable(const std::string &path) const {
  const auto resolved =
      KSMVirtualPath::ResolveWritable(path, data_root_, cache_root_, temp_root_);
  if (!resolved) {
    return std::nullopt;
  }
  const std::filesystem::path &root =
      path.starts_with("app://data/")    ? data_root_
      : path.starts_with("app://cache/") ? cache_root_
                                         : temp_root_;
  return ValidateContainedTarget(root, *resolved, false);
}

std::optional<std::filesystem::path>
KSMFileSystem::ResolveReadableDirectory(const std::string &path) const {
  const auto resolved = KSMVirtualPath::ResolveReadableDirectory(
      path, data_root_, cache_root_, temp_root_, resource_root_, asset_root_);
  if (!resolved) {
    return std::nullopt;
  }
  const std::filesystem::path *root = nullptr;
  const std::string normalized = KSMVirtualPath::NormalizeDirectory(path);
  if (normalized == "app://data" || normalized.starts_with("app://data/")) {
    root = &data_root_;
  } else if (
      normalized == "app://cache" || normalized.starts_with("app://cache/")
  ) {
    root = &cache_root_;
  } else if (
      normalized == "app://tmp" || normalized.starts_with("app://tmp/")
  ) {
    root = &temp_root_;
  } else if (normalized.starts_with("app://res")) {
    root = &resource_root_;
  } else if (normalized.starts_with("app://assets")) {
    root = &asset_root_;
  }
  return root ? ValidateContainedTarget(*root, *resolved, true) : std::nullopt;
}

std::optional<std::filesystem::path>
KSMFileSystem::ResolveWritableDirectory(const std::string &path) const {
  const auto resolved = KSMVirtualPath::ResolveWritableDirectory(
      path, data_root_, cache_root_, temp_root_);
  if (!resolved) {
    return std::nullopt;
  }
  const std::string normalized = KSMVirtualPath::NormalizeDirectory(path);
  const std::filesystem::path &root =
      normalized == "app://data" || normalized.starts_with("app://data/")
          ? data_root_
          : (normalized == "app://cache" ||
             normalized.starts_with("app://cache/"))
                ? cache_root_
                : temp_root_;
  std::error_code error;
  const bool exists = std::filesystem::exists(*resolved, error) && !error;
  return ValidateContainedTarget(root, *resolved, exists);
}

std::optional<std::string>
KSMFileSystem::ReadText(const std::string &path) const {
  const auto resolved = ResolveReadable(path);
  if (resolved && std::filesystem::is_regular_file(*resolved)) {
    std::error_code size_error;
    const auto size = std::filesystem::file_size(*resolved, size_error);
    if (size_error || size > kMaxTextBytes) {
      return std::nullopt;
    }
    std::ifstream input(*resolved, std::ios::binary);
    if (!input) {
      return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
  }
  if (!read_only_resource_provider_) {
    return std::nullopt;
  }
  const auto root = KSMVirtualPath::ReadOnlyRootFor(path);
  if (!root) {
    return std::nullopt;
  }
  const auto relative = KSMVirtualPath::ReadOnlyRelativePath(path, *root);
  if (!relative) {
    return std::nullopt;
  }
  const auto resource = read_only_resource_provider_(
      *root == KSMVirtualPath::ReadOnlyRoot::Asset, *relative);
  if (!resource || resource->data.size() > kMaxTextBytes) {
    return std::nullopt;
  }
  return std::string(resource->data.begin(), resource->data.end());
}

bool KSMFileSystem::WriteText(const std::string &path,
                              const std::string &content) const {
  if (content.size() > kMaxTextBytes) {
    return false;
  }
  const auto resolved = ResolveWritableFile(path);
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

bool KSMFileSystem::WriteTextAtomic(
    const std::string &path,
    const std::string &content
) const {
    if (content.size() > kMaxTextBytes) {
        return false;
    }
    const auto target = ResolveWritableFile(path);
    if (!target) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(target->parent_path(), error);
    if (error) {
        return false;
    }
    const std::filesystem::path temporary =
        target->parent_path() /
        (target->filename().wstring() + L".tmp-" +
         std::to_wstring(next_temp_id_.fetch_add(1)));
    const HANDLE handle = CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH,
        nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::size_t offset = 0;
    bool written = true;
    while (offset < content.size()) {
        const DWORD request = static_cast<DWORD>(
            std::min<std::size_t>(
                content.size() - offset,
                static_cast<std::size_t>(0x7fffffff)
            )
        );
        DWORD completed = 0;
        if (
            !WriteFile(handle, content.data() + offset, request, &completed, nullptr) ||
            completed == 0
        ) {
            written = false;
            break;
        }
        offset += completed;
    }
    if (written) {
        written = FlushFileBuffers(handle) != FALSE;
    }
    CloseHandle(handle);
    if (
        !written ||
        !MoveFileExW(
            temporary.c_str(),
            target->c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )
    ) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

bool KSMFileSystem::Exists(const std::string &path) const {
  const auto resolved = ResolveReadable(path);
  if (resolved && std::filesystem::exists(*resolved)) {
    return true;
  }
  return StatBundled(path).has_value();
}

bool KSMFileSystem::Remove(const std::string &path) const {
  const auto resolved = ResolveWritable(path);
  if (!resolved) {
    return false;
  }
  std::error_code error;
  return std::filesystem::remove(*resolved, error) && !error;
}

std::optional<std::vector<KSMDirectoryEntry>>
KSMFileSystem::ReadDir(const std::string &path) const {
  const auto resolved = ResolveReadableDirectory(path);
  if (!resolved) {
    return ReadBundledDirectory(path);
  }
  std::error_code error;
  const auto link_status = std::filesystem::symlink_status(*resolved, error);
  if (error || link_status.type() == std::filesystem::file_type::symlink ||
      !std::filesystem::is_directory(link_status)) {
    return std::nullopt;
  }

  std::vector<KSMDirectoryEntry> entries;
  std::filesystem::directory_iterator iterator(
      *resolved, std::filesystem::directory_options::skip_permission_denied,
      error);
  if (error) {
    return std::nullopt;
  }
  const std::filesystem::directory_iterator end;
  while (iterator != end) {
    const auto &entry = *iterator;
    const auto metadata = ReadMetadata(entry.path());
    if (metadata) {
      const std::string name = ToUtf8(entry.path().filename());
      if (!name.empty()) {
        entries.push_back(
            KSMDirectoryEntry{name, VirtualChildPath(path, name), *metadata});
      }
    }
    iterator.increment(error);
    if (error) {
      return std::nullopt;
    }
  }
  std::sort(entries.begin(), entries.end(),
            [](const KSMDirectoryEntry &left, const KSMDirectoryEntry &right) {
              return left.name < right.name;
            });
  return entries;
}

std::optional<bool> KSMFileSystem::MakeDirectory(const std::string &path,
                                                 bool recursive) const {
  const auto resolved = ResolveWritableDirectory(path);
  if (!resolved) {
    return std::nullopt;
  }
  std::error_code error;
  const auto status = std::filesystem::symlink_status(*resolved, error);
  if (!error && std::filesystem::exists(status)) {
    if (status.type() == std::filesystem::file_type::symlink ||
        !std::filesystem::is_directory(status)) {
      return std::nullopt;
    }
    return false;
  }
  error.clear();
  const bool created =
      recursive ? std::filesystem::create_directories(*resolved, error)
                : std::filesystem::create_directory(*resolved, error);
  if (error) {
    return std::nullopt;
  }
  if (created) {
    return true;
  }
  const auto final_status = std::filesystem::symlink_status(*resolved, error);
  if (!error && std::filesystem::is_directory(final_status) &&
      final_status.type() != std::filesystem::file_type::symlink) {
    return false;
  }
  return std::nullopt;
}

std::optional<KSMFileMetadata>
KSMFileSystem::Stat(const std::string &path) const {
  auto resolved = ResolveReadableDirectory(path);
  if (!resolved) {
    resolved = ResolveReadable(path);
  }
  if (!resolved) {
    return StatBundled(path);
  }
  const auto metadata = ReadMetadata(*resolved);
  return metadata ? metadata : StatBundled(path);
}

std::optional<std::vector<KSMDirectoryEntry>>
KSMFileSystem::ReadBundledDirectory(const std::string &path) const {
  if (!read_only_directory_provider_) {
    return std::nullopt;
  }
  const auto root = KSMVirtualPath::ReadOnlyRootFor(path);
  if (!root) {
    return std::nullopt;
  }
  const std::string normalized = KSMVirtualPath::NormalizeDirectory(path);
  const std::string root_path = *root == KSMVirtualPath::ReadOnlyRoot::Resource
                                    ? "app://res"
                                    : "app://assets";
  const auto relative =
      normalized == root_path
          ? std::optional<std::string>(std::string())
          : KSMVirtualPath::ReadOnlyRelativePath(normalized, *root);
  if (!relative) {
    return std::nullopt;
  }
  const auto names = read_only_directory_provider_(
      *root == KSMVirtualPath::ReadOnlyRoot::Asset, *relative);
  std::vector<KSMDirectoryEntry> entries;
  for (const auto &name : names) {
    const std::string child = relative->empty() ? name : *relative + "/" + name;
    const auto metadata = StatBundled(
        std::string(root_path == "app://res" ? "app://res/" : "app://assets/") +
        child);
    if (!metadata) {
      continue;
    }
    entries.push_back(
        KSMDirectoryEntry{name, VirtualChildPath(path, name), *metadata});
  }
  return entries;
}

std::optional<KSMFileMetadata>
KSMFileSystem::StatBundled(const std::string &path) const {
  if (!read_only_resource_provider_) {
    return std::nullopt;
  }
  const auto root = KSMVirtualPath::ReadOnlyRootFor(path);
  if (!root) {
    return std::nullopt;
  }
  const std::string normalized = KSMVirtualPath::NormalizeDirectory(path);
  const std::string root_path = *root == KSMVirtualPath::ReadOnlyRoot::Resource
                                    ? "app://res"
                                    : "app://assets";
  if (normalized == root_path) {
    return KSMFileMetadata{KSMFileKind::Directory, 0, std::nullopt};
  }
  const auto relative = KSMVirtualPath::ReadOnlyRelativePath(normalized, *root);
  if (!relative) {
    return std::nullopt;
  }
  const auto resource = read_only_resource_provider_(
      *root == KSMVirtualPath::ReadOnlyRoot::Asset, *relative);
  if (!resource) {
    return std::nullopt;
  }
  return KSMFileMetadata{KSMFileKind::File, resource->data.size(),
                         std::nullopt};
}

std::optional<std::string>
KSMFileSystem::CreateTemporaryText(const std::string &content) const {
  PruneTemporaryText(content.size());
  const std::string virtual_path =
      "app://tmp/read-" + std::to_string(next_temp_id_.fetch_add(1)) + ".txt";
  if (!WriteText(virtual_path, content)) {
    return std::nullopt;
  }
  return virtual_path;
}

void KSMFileSystem::PruneTemporaryText(std::size_t incoming_bytes) const {
  constexpr auto kMaxTemporaryBytes = 64u * 1024u * 1024u;
  constexpr auto kMaxAge = std::chrono::hours(24);
  struct Candidate {
    std::filesystem::path path;
    std::filesystem::file_time_type modified;
    std::uintmax_t size;
  };
  std::error_code error;
  std::vector<Candidate> candidates;
  std::uintmax_t total = 0;
  const auto now = std::filesystem::file_time_type::clock::now();
  std::filesystem::directory_iterator iterator(
      temp_root_, std::filesystem::directory_options::skip_permission_denied,
      error);
  if (error) {
    return;
  }
  for (const auto &entry : iterator) {
    const std::string filename = entry.path().filename().string();
    if (!filename.starts_with("read-") || !filename.ends_with(".txt")) {
      continue;
    }
    const auto link_status = std::filesystem::symlink_status(entry.path(), error);
    if (error || link_status.type() == std::filesystem::file_type::symlink ||
        !std::filesystem::is_regular_file(link_status)) {
      error.clear();
      continue;
    }
    const auto size = std::filesystem::file_size(entry.path(), error);
    const auto modified = std::filesystem::last_write_time(entry.path(), error);
    if (error) {
      error.clear();
      continue;
    }
    if (now - modified > kMaxAge) {
      std::filesystem::remove(entry.path(), error);
      error.clear();
      continue;
    }
    total += size;
    candidates.push_back(Candidate{entry.path(), modified, size});
  }
  std::sort(
      candidates.begin(), candidates.end(),
      [](const Candidate &left, const Candidate &right) {
        return left.modified < right.modified;
      });
  for (const auto &candidate : candidates) {
    if (total + incoming_bytes <= kMaxTemporaryBytes) {
      break;
    }
    std::filesystem::remove(candidate.path, error);
    if (!error) {
      total -= candidate.size;
    }
    error.clear();
  }
}

std::optional<std::filesystem::path>
KSMFileSystem::ResolveWritableFile(const std::string &path) const {
  const auto resolved = ResolveWritable(path);
  if (!resolved) {
    return std::nullopt;
  }

  std::error_code error;
  const auto link_status = std::filesystem::symlink_status(*resolved, error);
  if (!error && link_status.type() == std::filesystem::file_type::symlink) {
    return std::nullopt;
  }
  if (!error && std::filesystem::is_directory(link_status)) {
    return std::nullopt;
  }

  const std::filesystem::path root =
      path.starts_with("app://data/")    ? data_root_
      : path.starts_with("app://cache/") ? cache_root_
                                         : temp_root_;
  const auto canonical_root = std::filesystem::weakly_canonical(root, error);
  if (error) {
    return std::nullopt;
  }
  const auto canonical_parent =
      std::filesystem::weakly_canonical(resolved->parent_path(), error);
  if (error || !IsWithin(canonical_parent, canonical_root)) {
    return std::nullopt;
  }
  return resolved;
}
