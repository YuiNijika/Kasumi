#include "ksm_web_resource.h"

#include <string>

namespace {

constexpr char kAppLocalPrefix[] = "https://app.local";

int HexValue(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

std::optional<std::string> DecodePath(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char current = value[index];
    if (current != '%') {
      result.push_back(current);
      continue;
    }
    if (index + 2 >= value.size()) {
      return std::nullopt;
    }
    const int high = HexValue(value[index + 1]);
    const int low = HexValue(value[index + 2]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    const char decoded = static_cast<char>((high << 4) | low);
    if (decoded == '\0') {
      return std::nullopt;
    }
    result.push_back(decoded);
    index += 2;
  }
  return result;
}

bool IsSafeResourcePath(const std::string &path) {
  if (path.empty() || path.starts_with('/') ||
      path.find('\0') != std::string::npos ||
      path.find('\\') != std::string::npos ||
      path.find(':') != std::string::npos) {
    return false;
  }
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t end = path.find('/', start);
    const std::string segment = path.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (segment.empty() || segment == "." || segment == "..") {
      return false;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return true;
}

} // namespace

std::optional<std::string>
KSMWebResourcePath::FromAppLocalUrl(const std::string &url) {
  if (!url.starts_with(kAppLocalPrefix)) {
    return std::nullopt;
  }
  std::string remainder = url.substr(sizeof(kAppLocalPrefix) - 1);
  if (!remainder.empty() && remainder.front() != '/') {
    return std::nullopt;
  }
  const std::size_t query = remainder.find_first_of("?#");
  if (query != std::string::npos) {
    remainder.resize(query);
  }
  if (remainder.empty() || remainder == "/") {
    return std::string("index.html");
  }

  const auto decoded = DecodePath(remainder.substr(1));
  if (!decoded) {
    return std::nullopt;
  }
  std::string path = *decoded;
  if (path.empty()) {
    return std::string("index.html");
  }
  if (path.ends_with('/')) {
    path += "index.html";
  }
  if (!IsSafeResourcePath(path)) {
    return std::nullopt;
  }
  return path;
}
