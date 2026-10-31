#include "ksm_win32_resource_provider.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

KSMWin32ResourceProvider::KSMWin32ResourceProvider(
    HINSTANCE module, std::vector<KSMWin32ResourceDescriptor> resources)
    : module_(module == nullptr ? GetModuleHandleW(nullptr) : module),
      resources_(std::move(resources)) {}

std::optional<KSMWebResource>
KSMWin32ResourceProvider::Read(const std::string &path) const {
  const auto resource =
      std::find_if(resources_.begin(), resources_.end(),
                   [&path](const KSMWin32ResourceDescriptor &candidate) {
                     return candidate.path == path;
                   });
  if (resource == resources_.end()) {
    return std::nullopt;
  }

  const HRSRC handle = FindResourceW(
      module_, MAKEINTRESOURCEW(resource->resource_id), RT_RCDATA);
  if (handle == nullptr) {
    return std::nullopt;
  }
  const DWORD size = SizeofResource(module_, handle);
  if (size == 0) {
    return KSMWebResource{resource->mime_type, {}};
  }
  const HGLOBAL loaded = LoadResource(module_, handle);
  if (loaded == nullptr) {
    return std::nullopt;
  }
  const auto *data = static_cast<const std::uint8_t *>(LockResource(loaded));
  if (data == nullptr) {
    return std::nullopt;
  }

  KSMWebResource result;
  result.mime_type = resource->mime_type;
  result.data.resize(size);
  std::memcpy(result.data.data(), data, size);
  return result;
}

std::vector<std::string>
KSMWin32ResourceProvider::List(const std::string &directory) const {
  const std::string prefix =
      directory.empty() ? std::string() : directory + "/";
  std::set<std::string> names;
  for (const auto &resource : resources_) {
    if (!resource.path.starts_with(prefix)) {
      continue;
    }
    const std::string remainder = resource.path.substr(prefix.size());
    if (remainder.empty()) {
      continue;
    }
    const std::size_t separator = remainder.find('/');
    names.insert(remainder.substr(0, separator));
  }
  return {names.begin(), names.end()};
}
