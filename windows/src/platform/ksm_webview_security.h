#pragma once

#include <string>
#include <string_view>

class KSMWebViewSecurity {
public:
    static constexpr std::string_view ContentSecurityPolicy() {
        return "default-src 'self'; script-src 'self'; style-src 'self'; "
               "img-src 'self' data:; connect-src 'self'; object-src 'none'; "
               "base-uri 'none'; frame-ancestors 'none'";
    }

    static bool IsAllowedOrigin(const std::string& origin);
    static bool IsAllowedNavigation(const std::string& url);
};
