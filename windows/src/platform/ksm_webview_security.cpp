#include "ksm_webview_security.h"

#include <string>

namespace {

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.starts_with(prefix);
}

}  // namespace

bool KSMWebViewSecurity::IsAllowedOrigin(
    const std::string& origin,
    bool allow_development_origin
) {
    return origin == "https://app.local" ||
        StartsWith(origin, "https://app.local/") ||
        (allow_development_origin &&
            (origin == "http://localhost:5173" ||
                StartsWith(origin, "http://localhost:5173/")));
}

bool KSMWebViewSecurity::IsAllowedNavigation(
    const std::string& url,
    bool allow_development_origin
) {
    return url == "https://app.local" ||
        StartsWith(url, "https://app.local/") ||
        (allow_development_origin &&
            (url == "http://localhost:5173" ||
                StartsWith(url, "http://localhost:5173/")));
}
