#include "ksm_url_policy.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>

namespace {

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );
    return result;
}

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size
    );
    return result;
}

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

bool IsSchemeCharacter(char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 ||
        character == '+' ||
        character == '-' ||
        character == '.';
}

bool HasAbsoluteScheme(const std::string& value) {
    const auto separator = value.find("://");
    if (separator == std::string::npos || separator == 0) {
        return false;
    }
    return std::all_of(
        value.begin(),
        value.begin() + static_cast<std::ptrdiff_t>(separator),
        IsSchemeCharacter
    );
}

std::string NormalizePath(const std::string& path) {
    std::string source = path;
    if (source.empty() || source.front() != '/') {
        source.insert(source.begin(), '/');
    }

    const bool trailing_slash = source.size() > 1 && source.back() == '/';
    std::string result;
    std::size_t position = 0;
    while (position < source.size()) {
        while (position < source.size() && source[position] == '/') {
            ++position;
        }
        const std::size_t end = source.find('/', position);
        const std::string segment = source.substr(
            position,
            end == std::string::npos ? std::string::npos : end - position
        );
        if (!segment.empty() && segment != ".") {
            if (segment == "..") {
                if (!result.empty()) {
                    const auto slash = result.find_last_of('/');
                    result.erase(slash == std::string::npos ? 0 : slash);
                }
            } else {
                result.push_back('/');
                result.append(segment);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        position = end + 1;
    }
    if (result.empty()) {
        result = "/";
    }
    if (trailing_slash && result.back() != '/') {
        result.push_back('/');
    }
    return result;
}

std::string StripFragment(const std::string& value) {
    const auto fragment = value.find('#');
    return fragment == std::string::npos
        ? value
        : value.substr(0, fragment);
}

std::string BuildUrl(
    const KSMUrlPolicy::Origin& origin,
    const std::string& path,
    const std::string& query
) {
    std::string result = origin.scheme + "://" + origin.host;
    const bool is_default_port =
        (origin.scheme == "https" && origin.port == 443) ||
        (origin.scheme == "http" && origin.port == 80);
    if (!is_default_port) {
        result += ":" + std::to_string(origin.port);
    }
    result += path.empty() ? "/" : path;
    if (!query.empty()) {
        result += query.front() == '?' ? query : "?" + query;
    }
    return result;
}

}

bool KSMUrlPolicy::Origin::operator<(
    const Origin& other
) const {
    if (scheme != other.scheme) {
        return scheme < other.scheme;
    }
    if (host != other.host) {
        return host < other.host;
    }
    return port < other.port;
}

bool KSMUrlPolicy::Origin::operator==(
    const Origin& other
) const {
    return scheme == other.scheme &&
        host == other.host &&
        port == other.port;
}

std::optional<KSMUrlPolicy::ParsedUrl> KSMUrlPolicy::ParseHttpUrl(
    const std::string& url,
    bool https_only
) {
    if (url.empty() || url.size() > kMaxUrlLength) {
        return std::nullopt;
    }
    const std::wstring wide_url = ToWide(url);
    if (wide_url.empty()) {
        return std::nullopt;
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUserNameLength = static_cast<DWORD>(-1);
    components.dwPasswordLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &components)) {
        return std::nullopt;
    }
    if (
        components.dwSchemeLength == 0 ||
        components.dwHostNameLength == 0 ||
        components.dwUserNameLength > 0 ||
        components.dwPasswordLength > 0
    ) {
        return std::nullopt;
    }

    const std::string scheme = LowerAscii(ToUtf8(std::wstring(
        components.lpszScheme,
        components.dwSchemeLength
    )));
    if (
        scheme != "http" &&
        scheme != "https"
    ) {
        return std::nullopt;
    }
    if (https_only && scheme != "https") {
        return std::nullopt;
    }
    if (components.nPort == 0 || components.nPort > 65535) {
        return std::nullopt;
    }

    std::string path;
    if (components.lpszUrlPath && components.dwUrlPathLength > 0) {
        path = ToUtf8(std::wstring(
            components.lpszUrlPath,
            components.dwUrlPathLength
        ));
    }
    path = NormalizePath(StripFragment(path));

    std::string query;
    if (components.lpszExtraInfo && components.dwExtraInfoLength > 0) {
        const std::string extra = StripFragment(ToUtf8(std::wstring(
            components.lpszExtraInfo,
            components.dwExtraInfoLength
        )));
        if (!extra.empty()) {
            query = extra;
        }
    }

    return ParsedUrl{
        Origin{
            scheme,
            LowerAscii(ToUtf8(std::wstring(
                components.lpszHostName,
                components.dwHostNameLength
            ))),
            static_cast<int>(components.nPort)
        },
        std::move(path),
        std::move(query)
    };
}

std::optional<KSMUrlPolicy::Origin> KSMUrlPolicy::ParseOrigin(
    const std::string& value
) {
    const auto parsed = ParseHttpUrl(value);
    if (!parsed) {
        return std::nullopt;
    }
    if (
        parsed->path != "/" ||
        !parsed->query.empty()
    ) {
        return std::nullopt;
    }
    return parsed->origin;
}

std::set<KSMUrlPolicy::Origin> KSMUrlPolicy::NormalizeOrigins(
    const std::vector<std::string>& values
) {
    std::set<Origin> result;
    for (const auto& value : values) {
        if (const auto origin = ParseOrigin(value)) {
            result.insert(*origin);
        }
    }
    return result;
}

bool KSMUrlPolicy::IsAllowedExternalUrl(
    const std::string& url,
    const std::set<Origin>& allowed_origins
) {
    const auto parsed = ParseHttpUrl(url);
    return parsed && allowed_origins.find(parsed->origin) != allowed_origins.end();
}

std::optional<std::string> KSMUrlPolicy::ResolveHttpsRedirect(
    const std::string& current_url,
    const std::string& location
) {
    const auto current = ParseHttpUrl(current_url, true);
    if (!current || location.empty() || location.size() > kMaxUrlLength) {
        return std::nullopt;
    }

    std::string target;
    if (HasAbsoluteScheme(location)) {
        target = location;
    } else if (location.starts_with("//")) {
        target = "https:" + location;
    } else {
        const std::string cleaned = StripFragment(location);
        if (cleaned.starts_with("?")) {
            target = BuildUrl(current->origin, current->path, cleaned);
        } else if (cleaned.starts_with("/")) {
            const auto query_at = cleaned.find('?');
            target = BuildUrl(
                current->origin,
                NormalizePath(cleaned.substr(0, query_at)),
                query_at == std::string::npos
                    ? ""
                    : cleaned.substr(query_at)
            );
        } else {
            const auto query_at = cleaned.find('?');
            const std::string relative_path = cleaned.substr(0, query_at);
            const auto slash = current->path.find_last_of('/');
            const std::string base = slash == std::string::npos
                ? "/"
                : current->path.substr(0, slash + 1);
            target = BuildUrl(
                current->origin,
                NormalizePath(base + relative_path),
                query_at == std::string::npos
                    ? ""
                    : cleaned.substr(query_at)
            );
        }
    }

    if (!ParseHttpUrl(target, true)) {
        return std::nullopt;
    }
    return target;
}
