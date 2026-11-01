#pragma once

#include <string>
#include <string_view>

class KSMWebViewSecurity {
public:
    static constexpr std::string_view ContentSecurityPolicy() {
        return "default-src 'self'; script-src 'self' 'unsafe-inline'; "
               "style-src 'self' 'unsafe-inline'; "
               "img-src 'self' data:; connect-src 'self'; object-src 'none'; "
               "base-uri 'none'; frame-ancestors 'none'";
    }

    static bool IsAllowedOrigin(
        const std::string& origin,
        bool allow_development_origin = false
    );
    static bool IsAllowedNavigation(
        const std::string& url,
        bool allow_development_origin = false
    );
};
