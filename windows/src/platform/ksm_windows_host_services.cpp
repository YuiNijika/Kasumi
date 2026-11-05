#include "ksm_windows_host_services.h"

#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <winhttp.h>
#include <winternl.h>
#include <windns.h>
#include <wrl/client.h>
#include <shobjidl_core.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include <nlohmann/json.hpp>
#include "../platform/ksm_virtual_path.h"
#include "ksm_odbc_sql_driver.h"
#include "ksm_sqlite_driver.h"

namespace {

constexpr std::size_t kMaxDownloadBytes = 100 * 1024 * 1024;
constexpr std::size_t kDownloadBufferBytes = 64 * 1024;
constexpr int kDownloadTimeoutMs = 120000;
constexpr int kDownloadIoTimeoutMs = 15000;
constexpr int kMaxRedirects = 5;
constexpr int kMinWindowWidth = 320;
constexpr int kMinWindowHeight = 240;
constexpr std::size_t kMaxInlineResponseBytes = 60 * 1024;
constexpr std::size_t kMaxRequestResponseBytes = 100 * 1024 * 1024;
constexpr DWORD kTrayCallbackMessage = WM_APP + 0x4C;
constexpr UINT kFirstHotkeyId = 0x4B60;
constexpr wchar_t kAudioAlias[] = L"kasumi_audio";

std::atomic<std::uint64_t> next_download_part{1};

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

bool MciCommand(const std::wstring& command) {
    return mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0;
}

std::optional<std::int64_t> MciStatusMilliseconds(
    const wchar_t* status
) {
    wchar_t buffer[64]{};
    if (
        mciSendStringW(
            (std::wstring(L"status kasumi_audio ") + status).c_str(),
            buffer,
            ARRAYSIZE(buffer),
            nullptr
        ) != 0
    ) {
        return std::nullopt;
    }
    wchar_t* end = nullptr;
    const long long value = std::wcstoll(buffer, &end, 10);
    if (end == buffer) {
        return std::nullopt;
    }
    return value;
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

bool IsForbiddenHeader(const std::string& name) {
    static const std::set<std::string> blocked = {
        "cookie",
        "host",
        "proxy-authorization",
        "set-cookie"
    };
    return blocked.contains(LowerAscii(name));
}

std::optional<std::wstring> BuildRequestHeaders(
    const std::vector<KSMHttpHeader>& headers,
    const std::optional<std::string>& user_agent,
    const std::optional<std::string>& referer,
    const std::string& cookie_header
) {
    std::wstring result;
    const auto append = [&result](
        const std::string& name,
        const std::string& value
    ) {
        if (
            name.empty() ||
            IsForbiddenHeader(name) ||
            value.find('\r') != std::string::npos ||
            value.find('\n') != std::string::npos
        ) {
            return false;
        }
        result += ToWide(name) + L": " + ToWide(value) + L"\r\n";
        return true;
    };
    if (user_agent && !append("User-Agent", *user_agent)) {
        return std::nullopt;
    }
    if (referer && !append("Referer", *referer)) {
        return std::nullopt;
    }
    for (const auto& header : headers) {
        const std::string name = LowerAscii(header.name);
        if (
            name == "user-agent" ||
            name == "referer" ||
            !append(header.name, header.value)
        ) {
            return std::nullopt;
        }
    }
    if (!cookie_header.empty()) {
        result += L"Cookie: " + ToWide(cookie_header) + L"\r\n";
    }
    return result;
}

std::string CookieHeader(
    const std::vector<KSMCookie>& cookies
) {
    std::string result;
    for (const auto& cookie : cookies) {
        if (!result.empty()) {
            result += "; ";
        }
        result += cookie.name + "=" + cookie.value;
    }
    return result;
}

std::optional<std::string> QueryHeader(
    HINTERNET request,
    DWORD header
) {
    const auto value = QueryRequestHeader(request, header);
    if (!value) {
        return std::nullopt;
    }
    const std::string utf8 = ToUtf8(*value);
    return utf8.empty() ? std::nullopt : std::optional<std::string>(utf8);
}

std::optional<std::string> ExtractSetCookie(
    HINTERNET request,
    std::size_t index
) {
    DWORD size = 0;
    WinHttpQueryHeaders(
        request,
        WINHTTP_HEADER_SET_COOKIE,
        WINHTTP_HEADER_NAME_BY_INDEX,
        nullptr,
        &size,
        WINHTTP_NO_HEADER_INDEX
    );
    if (size == 0) {
        return std::nullopt;
    }
    std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_HEADER_SET_COOKIE,
            WINHTTP_HEADER_NAME_BY_INDEX,
            buffer.data(),
            &size,
            static_cast<DWORD>(index)
        )) {
        return std::nullopt;
    }
    return ToUtf8(std::wstring(buffer.data()));
}

KSMCookie ParseSetCookie(const std::string& value, const std::string& url) {
    const auto parsed = KSMUrlPolicy::ParseHttpUrl(url, true);
    const std::string pair = value.substr(0, value.find(';'));
    const auto separator = pair.find('=');
    KSMCookie cookie;
    cookie.domain = parsed ? parsed->origin.host : "";
    cookie.path = parsed ? parsed->path : "/";
    if (separator != std::string::npos) {
        cookie.name = pair.substr(0, separator);
        cookie.value = pair.substr(separator + 1);
    }
    cookie.http_only = LowerAscii(value).find("httponly") != std::string::npos;
    cookie.secure = LowerAscii(value).find("secure") != std::string::npos;
    return cookie;
}

std::optional<std::filesystem::path> ResolveScopeTarget(
    const std::filesystem::path& root,
    const std::string& virtual_path
) {
    const std::string prefix = "app://scope/";
    if (!virtual_path.starts_with(prefix)) {
        return std::nullopt;
    }
    const std::string remainder = virtual_path.substr(prefix.size());
    const auto slash = remainder.find('/');
    if (slash == std::string::npos || slash + 1 >= remainder.size()) {
        return std::nullopt;
    }
    const std::string relative = remainder.substr(slash + 1);
    if (
        relative.empty() ||
        relative.find("..") != std::string::npos ||
        relative.find('\0') != std::string::npos ||
        relative.find('\\') != std::string::npos ||
        relative.find(':') != std::string::npos ||
        relative.starts_with('/')
    ) {
        return std::nullopt;
    }
    std::error_code error;
    const auto canonical_root = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return std::nullopt;
    }
    const auto target = std::filesystem::weakly_canonical(
        root / std::filesystem::path(relative),
        error
    );
    if (error) {
        return std::nullopt;
    }
    const auto root_text = canonical_root.generic_u8string();
    const auto target_text = target.generic_u8string();
    if (
        target_text.size() < root_text.size() ||
        target_text.compare(0, root_text.size(), root_text) != 0
    ) {
        return std::nullopt;
    }
    return target;
}

std::optional<std::pair<UINT, std::string>> ParseHotkey(
    const std::string& shortcut
) {
    static const std::unordered_map<std::string, UINT> modifiers = {
        {"ctrl", MOD_CONTROL},
        {"control", MOD_CONTROL},
        {"alt", MOD_ALT},
        {"shift", MOD_SHIFT},
        {"win", MOD_WIN}
    };
    UINT modifier_value = 0;
    std::string key;
    std::istringstream stream(shortcut);
    std::string token;
    while (std::getline(stream, token, '+')) {
        const std::string normalized = LowerAscii(token);
        const auto modifier = modifiers.find(normalized);
        if (modifier != modifiers.end()) {
            modifier_value |= modifier->second;
            continue;
        }
        if (!key.empty() || token.size() != 1) {
            return std::nullopt;
        }
        key = token;
    }
    if (key.empty() || modifier_value == 0) {
        return std::nullopt;
    }
    const char character = static_cast<char>(std::toupper(key[0]));
    if (
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9')
    ) {
        return std::make_pair(
            modifier_value,
            std::string(1, character)
        );
    }
    return std::nullopt;
}

bool IsPublicIpv4(const unsigned char* bytes) {
    const unsigned int first = bytes[0];
    const unsigned int second = bytes[1];
    return first != 0 &&
        first != 10 &&
        first != 127 &&
        !(first == 169 && second == 254) &&
        !(first == 172 && second >= 16 && second <= 31) &&
        !(first == 192 && second == 168) &&
        !(first == 100 && second >= 64 && second <= 127) &&
        first < 224;
}

bool IsPublicIpv6(const unsigned char* bytes) {
    const bool is_ipv4_mapped =
        std::all_of(bytes, bytes + 10, [](unsigned char byte) {
            return byte == 0;
        }) &&
        bytes[10] == 0xff &&
        bytes[11] == 0xff;
    if (is_ipv4_mapped) {
        return IsPublicIpv4(bytes + 12);
    }
    const bool is_unspecified = std::all_of(
        bytes,
        bytes + 16,
        [](unsigned char byte) { return byte == 0; }
    );
    const bool is_loopback = std::all_of(
        bytes,
        bytes + 15,
        [](unsigned char byte) { return byte == 0; }
    ) && bytes[15] == 1;
    return !is_unspecified &&
        !is_loopback &&
        (bytes[0] & 0xfe) != 0xfc &&
        !(bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) &&
        (bytes[0] & 0xf0) != 0xf0;
}

bool IsPublicHost(const std::string& host) {
    PDNS_RECORD records = nullptr;
    const std::wstring wide_host = ToWide(host);
    const DNS_STATUS status = DnsQuery_W(
        wide_host.c_str(),
        DNS_TYPE_ANY,
        DNS_QUERY_STANDARD,
        nullptr,
        &records,
        nullptr
    );
    if (status != ERROR_SUCCESS || !records) {
        if (records) {
            DnsRecordListFree(records, DnsFreeRecordList);
        }
        return false;
    }

    bool has_address = false;
    bool is_public = true;
    for (const DNS_RECORD* current = records; current && is_public;
         current = current->pNext) {
        if (current->wType == DNS_TYPE_A) {
            has_address = true;
            is_public = IsPublicIpv4(
                reinterpret_cast<const unsigned char*>(
                    &current->Data.A.IpAddress
                )
            );
        } else if (current->wType == DNS_TYPE_AAAA) {
            has_address = true;
            is_public = IsPublicIpv6(
                reinterpret_cast<const unsigned char*>(
                    &current->Data.AAAA.Ip6Address
                )
            );
        }
    }
    DnsRecordListFree(records, DnsFreeRecordList);
    return has_address && is_public;
}

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

std::wstring BuildFilter(const std::vector<std::string>& filters) {
    const std::wstring all_files = L"All Files";
    const std::wstring all_patterns = L"*.*";
    if (filters.empty()) {
        return all_files + L'\0' + all_patterns + L'\0' + L'\0';
    }

    std::wstring result;
    for (const auto& filter : filters) {
        const std::wstring wide_filter = ToWide(filter);
        if (wide_filter.empty()) {
            continue;
        }
        result.append(wide_filter);
        result.push_back(L'\0');
        result.append(wide_filter);
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    if (result.size() == 1) {
        return all_files + L'\0' + all_patterns + L'\0' + L'\0';
    }
    return result;
}

bool IsRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

bool IsDirectory(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

std::string HttpErrorText(const char* operation) {
    return std::string(operation) + " failed with WinHTTP error " +
        std::to_string(GetLastError()) + ".";
}

std::optional<std::wstring> QueryRequestHeader(
    HINTERNET request,
    DWORD header
) {
    DWORD size = 0;
    if (
        WinHttpQueryHeaders(
            request,
            header,
            WINHTTP_HEADER_NAME_BY_INDEX,
            nullptr,
            &size,
            WINHTTP_NO_HEADER_INDEX
        ) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER
    ) {
        return std::nullopt;
    }
    std::wstring value(
        (size / sizeof(wchar_t)) + 1,
        L'\0'
    );
    if (!WinHttpQueryHeaders(
        request,
        header,
        WINHTTP_HEADER_NAME_BY_INDEX,
        value.data(),
        &size,
        WINHTTP_NO_HEADER_INDEX
    )) {
        return std::nullopt;
    }
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

bool IsFullscreenWindow(HWND window) {
    RECT window_rect{};
    if (!GetWindowRect(window, &window_rect)) {
        return false;
    }
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return false;
    }
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const bool has_caption = (style & WS_CAPTION) == WS_CAPTION;
    return !has_caption &&
        window_rect.left <= info.rcMonitor.left &&
        window_rect.top <= info.rcMonitor.top &&
        window_rect.right >= info.rcMonitor.right &&
        window_rect.bottom >= info.rcMonitor.bottom;
}

}

KSMWindowsHostServices::KSMWindowsHostServices(
    HWND window,
    std::vector<std::string> allowed_external_origins,
    std::shared_ptr<KSMFileSystem> file_system,
    std::string app_version,
    std::string app_id
)
    : KSMWindowsHostServices(
          window,
          std::move(allowed_external_origins),
          {},
          std::move(file_system),
          std::move(app_version),
          std::move(app_id)
      ) {}

KSMWindowsHostServices::KSMWindowsHostServices(
    HWND window,
    std::vector<std::string> allowed_external_origins,
    std::vector<std::string> allowed_http_origins,
    std::shared_ptr<KSMFileSystem> file_system,
    std::string app_version,
    std::string app_id
)
    : window_(window),
      file_system_(std::move(file_system)),
      app_version_(std::move(app_version)),
      app_id_(std::move(app_id)),
      allowed_external_origins_(
          KSMUrlPolicy::NormalizeOrigins(allowed_external_origins)
      ),
      allowed_http_origins_(
          KSMUrlPolicy::NormalizeOrigins(allowed_http_origins)
      ) {
    RegisterSqlDriver(std::make_shared<KSMSqliteDriver>(file_system_));
    for (auto& driver : KSMOdbcSqlDriver::Discover()) {
        RegisterSqlDriver(std::move(driver));
    }
}

KSMWindowsHostServices::~KSMWindowsHostServices() {
    std::lock_guard lock(audio_mutex_);
    MciCommand(L"close kasumi_audio");
}

void KSMWindowsHostServices::SetEventDispatcher(
    std::function<void(const std::string&, const std::string&)> dispatcher
) {
    std::lock_guard lock(capability_mutex_);
    event_dispatcher_ = std::move(dispatcher);
}

void KSMWindowsHostServices::PushDeepLink(const std::string& url) {
    std::function<void(const std::string&, const std::string&)> dispatcher;
    {
        std::lock_guard lock(capability_mutex_);
        pending_deep_links_.push_back(url);
        dispatcher = event_dispatcher_;
    }
    if (dispatcher) {
        dispatcher(
            "app.deepLink.received",
            nlohmann::json{{"url", url}}.dump()
        );
    }
}

void KSMWindowsHostServices::DispatchCapabilityEvent(
    const std::string& event,
    const std::string& payload
) {
    std::function<void(const std::string&, const std::string&)> dispatcher;
    {
        std::lock_guard lock(capability_mutex_);
        dispatcher = event_dispatcher_;
    }
    if (dispatcher) {
        dispatcher(event, payload);
    }
}

void KSMWindowsHostServices::CloseTransferHandles(
    const std::shared_ptr<TransferState>& state
) {
    std::lock_guard lock(state->mutex);
    if (state->request) {
        WinHttpCloseHandle(state->request);
        state->request = nullptr;
    }
    if (state->connect) {
        WinHttpCloseHandle(state->connect);
        state->connect = nullptr;
    }
    if (state->session) {
        WinHttpCloseHandle(state->session);
        state->session = nullptr;
    }
}

std::optional<KSMHttpResponse> KSMWindowsHostServices::HttpRequest(
    const KSMHttpRequestSpec& request,
    const std::string& request_id
) {
    if (
        (request.jar_id && !IsHttpRequestUrlAllowed(request.url)) ||
        (request.method != "GET" && request.method != "POST") ||
        !KSMUrlPolicy::ParseHttpUrl(request.url, true)
    ) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The request URL or method is not allowed."
        );
    }
    const std::string jar_key = request.jar_id.value_or("");
    std::vector<KSMCookie> cookies;
    if (!jar_key.empty()) {
        std::lock_guard lock(capability_mutex_);
        const auto iterator = cookie_jars_.find(jar_key);
        if (iterator != cookie_jars_.end()) {
            cookies = iterator->second;
        }
    }
    const auto headers_value = BuildRequestHeaders(
        request.headers,
        request.user_agent,
        request.referer,
        CookieHeader(cookies)
    );
    if (!headers_value) {
        throw KSMHostException(
            "INVALID_ARGS",
            "The request headers are invalid."
        );
    }
    auto state = std::make_shared<TransferState>();
    state->request_id = request_id;
    std::string current_url = request.url;
    const auto parsed = KSMUrlPolicy::ParseHttpUrl(current_url, true);
    if (!parsed) {
        throw KSMHostException("INVALID_ARGS", "url must use HTTPS.");
    }
    if (!IsPublicHost(parsed->origin.host)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The request host is not a public network address."
        );
    }
    std::string active_url = current_url;
    const auto active_parsed = KSMUrlPolicy::ParseHttpUrl(active_url, true);
    const std::wstring host = ToWide(active_parsed->origin.host);
    const std::wstring path_and_query = ToWide(
        active_parsed->path + active_parsed->query
    );
    const std::wstring verb = ToWide(request.method);
    HINTERNET session = WinHttpOpen(
        L"Kasumi/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    HINTERNET connect = nullptr;
    HINTERNET http_request = nullptr;
    if (session) {
        connect = WinHttpConnect(
            session,
            host.c_str(),
            static_cast<INTERNET_PORT>(parsed->origin.port),
            0
        );
    }
    if (connect) {
        http_request = WinHttpOpenRequest(
            connect,
            verb.c_str(),
            path_and_query.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
    }
    {
        std::lock_guard lock(state->mutex);
        state->session = session;
        state->connect = connect;
        state->request = http_request;
    }
    if (!http_request) {
        CloseTransferHandles(state);
        throw KSMHostException(
            "IO_ERROR",
            "The HTTPS request could not be opened."
        );
    }
    DWORD disable_redirects = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(
        http_request,
        WINHTTP_OPTION_DISABLE_FEATURE,
        &disable_redirects,
        sizeof(disable_redirects)
    );
    const DWORD body_size = static_cast<DWORD>(request.body.size());
    for (int redirects = 0; ; ++redirects) {
        const auto parsed_url = KSMUrlPolicy::ParseHttpUrl(current_url, true);
        if (!parsed_url || redirects > kMaxRedirects) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                "The redirect target is invalid."
            );
        }
        if (
            !KSMUrlPolicy::IsAllowedDownloadUrl(
                current_url,
                allowed_http_origins_
            ) ||
            !IsPublicHost(parsed_url->origin.host)
        ) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The redirect origin is not allowlisted."
            );
        }
        if (!WinHttpSendRequest(
                http_request,
                headers_value->empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                    : headers_value->c_str(),
                headers_value->empty() ? 0 : static_cast<DWORD>(-1),
                request.body.empty() ? WINHTTP_NO_REQUEST_DATA
                    : const_cast<char*>(request.body.data()),
                body_size,
                body_size,
                0
            ) ||
            !WinHttpReceiveResponse(http_request, nullptr)) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                HttpErrorText("The HTTPS request")
            );
        }
        DWORD redirect_status = 0;
        DWORD redirect_status_size = sizeof(redirect_status);
        WinHttpQueryHeaders(
            http_request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &redirect_status,
            &redirect_status_size,
            WINHTTP_NO_HEADER_INDEX
        );
        if (redirect_status < 300 || redirect_status > 399) {
            break;
        }
        const auto location = QueryRequestHeader(
            http_request,
            WINHTTP_QUERY_LOCATION
        );
        const auto resolved = location
            ? KSMUrlPolicy::ResolveHttpsRedirect(
                current_url,
                ToUtf8(*location)
            )
            : std::nullopt;
        if (!resolved) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                "The redirect did not use HTTPS."
            );
        }
        current_url = *resolved;
        const auto redirect_parsed = KSMUrlPolicy::ParseHttpUrl(
            current_url,
            true
        );
        if (!redirect_parsed) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                "The redirect target is invalid."
            );
        }
        const std::wstring redirect_host = ToWide(
            redirect_parsed->origin.host
        );
        const std::wstring redirect_path = ToWide(
            redirect_parsed->path + redirect_parsed->query
        );
        HINTERNET redirect_session = WinHttpOpen(
            L"Kasumi/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        HINTERNET redirect_connect = redirect_session
            ? WinHttpConnect(
                redirect_session,
                redirect_host.c_str(),
                static_cast<INTERNET_PORT>(redirect_parsed->origin.port),
                0
            )
            : nullptr;
        HINTERNET redirect_request = redirect_connect
            ? WinHttpOpenRequest(
                redirect_connect,
                verb.c_str(),
                redirect_path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE
            )
            : nullptr;
        CloseTransferHandles(state);
        {
            std::lock_guard lock(state->mutex);
            state->session = redirect_session;
            state->connect = redirect_connect;
            state->request = redirect_request;
        }
        http_request = redirect_request;
        if (!http_request) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                "The HTTPS request could not be opened."
            );
        }
        DWORD redirect_disable = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(
            http_request,
            WINHTTP_OPTION_DISABLE_FEATURE,
            &redirect_disable,
            sizeof(redirect_disable)
        );
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(
        http_request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status,
        &status_size,
        WINHTTP_NO_HEADER_INDEX
    );
    for (std::size_t index = 0; ; ++index) {
        const auto set_cookie = ExtractSetCookie(http_request, index);
        if (!set_cookie || set_cookie->empty()) {
            break;
        }
        auto cookie = ParseSetCookie(*set_cookie, current_url);
        const auto existing = std::find_if(
            cookies.begin(),
            cookies.end(),
            [&cookie](const KSMCookie& value) {
                return value.name == cookie.name &&
                    value.domain == cookie.domain &&
                    value.path == cookie.path;
            }
        );
        if (existing != cookies.end()) {
            *existing = std::move(cookie);
        } else {
            cookies.push_back(std::move(cookie));
        }
    }
    if (!jar_key.empty()) {
        std::lock_guard lock(capability_mutex_);
        cookie_jars_[jar_key] = cookies;
    }
    KSMHttpResponse response;
    response.status = static_cast<int>(status);
    const auto content_type = QueryHeader(
        http_request,
        WINHTTP_QUERY_CONTENT_TYPE
    );
    if (content_type) {
        response.headers.push_back(
            KSMHttpHeader{"content-type", *content_type}
        );
    }
    std::string body;
    std::uint64_t total = 0;
    std::vector<char> buffer(kDownloadBufferBytes);
    const std::string staged_path =
        "app://tmp/http-" + std::to_string(
            next_download_part.fetch_add(1)
        ) + ".bin";
    std::optional<std::filesystem::path> staged_target;
    std::unique_ptr<std::ofstream> staged_output;
    if (file_system_) {
        staged_target = file_system_->ResolveWritableFile(staged_path);
        if (staged_target) {
            staged_output = std::make_unique<std::ofstream>(
                *staged_target,
                std::ios::binary | std::ios::trunc
            );
            if (!*staged_output) {
                staged_output.reset();
            }
        }
    }
    std::string key = request_id.empty()
        ? "anonymous-http-" + std::to_string(
            next_download_part.fetch_add(1)
        )
        : request_id;
    {
        std::lock_guard lock(capability_mutex_);
        if (transfers_.contains(key)) {
            throw KSMHostException(
                "BUSY",
                "A request with this ID is already running."
            );
        }
        transfers_.emplace(key, state);
    }
    while (true) {
        if (state->cancelled.load()) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "CANCELLED",
                "The request was cancelled."
            );
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(http_request, &available)) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                "The response could not be read."
            );
        }
        if (available == 0) {
            break;
        }
        DWORD read = 0;
        if (!WinHttpReadData(
                http_request,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &read
            )) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "IO_ERROR",
                "The response could not be read."
            );
        }
        total += read;
        if (total > kMaxRequestResponseBytes) {
            CloseTransferHandles(state);
            throw KSMHostException(
                "RESOURCE_LIMIT",
                "The response exceeds the configured limit."
            );
        }
        if (body.size() + read <= kMaxInlineResponseBytes) {
            body.append(buffer.data(), read);
        } else {
            body.clear();
        }
        if (staged_output) {
            staged_output->write(buffer.data(), read);
            if (!*staged_output) {
                staged_output.reset();
            }
        }
    }
    response.bytes = total;
    if (total <= kMaxInlineResponseBytes) {
        response.body = std::move(body);
    }
    if (total > kMaxInlineResponseBytes && staged_output && staged_target) {
        staged_output->flush();
        if (*staged_output) {
            response.file = staged_path;
        }
    }
    if (!response.file && total > kMaxInlineResponseBytes) {
        CloseTransferHandles(state);
        throw KSMHostException(
            "RESOURCE_LIMIT",
            "The large response could not be staged."
        );
    }
    CloseTransferHandles(state);
    {
        std::lock_guard lock(capability_mutex_);
        transfers_.erase(key);
    }
    return response;
}

std::optional<KSMHttpTransferStart>
KSMWindowsHostServices::StartHttpTransfer(
    const KSMHttpTransferSpec& request,
    const std::string& request_id
) {
    std::string transfer_id;
    auto state = std::make_shared<TransferState>();
    {
        std::lock_guard lock(capability_mutex_);
        transfer_id = "transfer-" + std::to_string(next_capability_id_++);
        transfers_.emplace(transfer_id, state);
    }
    state->request_id = request_id;
    DispatchCapabilityEvent(
        "http.transfer.progress",
        nlohmann::json{
            {"transferId", transfer_id},
            {"bytes", 0}
        }.dump()
    );
    std::thread([this, request, request_id, state, transfer_id]() {
        try {
            const auto response = HttpRequest(
                KSMHttpRequestSpec{
                    request.url,
                    request.method,
                    request.headers,
                    request.body,
                    request.jar_id,
                    request.user_agent,
                    request.referer
                },
                request_id
            );
            if (!response) {
                throw KSMHostException(
                    "NOT_SUPPORTED",
                    "The transfer is not implemented on this host."
                );
            }
            if (state->cancelled.load()) {
                throw KSMHostException(
                    "CANCELLED",
                    "The transfer was cancelled."
                );
            }
            if (!file_system_) {
                throw KSMHostException(
                    "NOT_SUPPORTED",
                    "The transfer file system is not configured."
                );
            }
            if (!request.overwrite && file_system_->Exists(request.path)) {
                throw KSMHostException(
                    "IO_ERROR",
                    "The transfer target already exists."
                );
            }
            bool written = false;
            if (response->file) {
                written = file_system_->Copy(*response->file, request.path);
            } else {
                written = file_system_->WriteBytes(
                    request.path,
                    std::vector<std::uint8_t>(
                        response->body.begin(),
                        response->body.end()
                    )
                );
            }
            if (!written) {
                throw KSMHostException(
                    "IO_ERROR",
                    "The transfer target could not be written."
                );
            }
            DispatchCapabilityEvent(
                "http.transfer.completed",
                nlohmann::json{
                    {"transferId", transfer_id},
                    {"status", response->status},
                    {"bytes", response->bytes},
                    {"file", request.path}
                }.dump()
            );
        } catch (const std::exception& error) {
            DispatchCapabilityEvent(
                "http.transfer.failed",
                nlohmann::json{
                    {"transferId", transfer_id},
                    {"message", error.what()}
                }.dump()
            );
        }
        std::lock_guard lock(capability_mutex_);
        transfers_.erase(transfer_id);
    }).detach();
    return KSMHttpTransferStart{transfer_id};
}

bool KSMWindowsHostServices::CancelHttpTransfer(
    const std::string& transfer_id
) {
    std::shared_ptr<TransferState> state;
    {
        std::lock_guard lock(capability_mutex_);
        const auto iterator = transfers_.find(transfer_id);
        if (iterator == transfers_.end()) {
            return false;
        }
        state = iterator->second;
    }
    state->cancelled.store(true);
    CloseTransferHandles(state);
    return true;
}

std::optional<std::vector<KSMCookie>>
KSMWindowsHostServices::GetCookies(
    const std::string& jar_id,
    const std::string& url
) {
    if (!IsHttpRequestUrlAllowed(url) || jar_id.empty()) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The URL origin is not allowlisted."
        );
    }
    std::lock_guard lock(capability_mutex_);
    return cookie_jars_[jar_id];
}

bool KSMWindowsHostServices::SetCookies(
    const std::string& jar_id,
    const std::string& url,
    const std::vector<KSMCookie>& cookies
) {
    if (!IsHttpRequestUrlAllowed(url) || jar_id.empty()) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The URL origin is not allowlisted."
        );
    }
    std::lock_guard lock(capability_mutex_);
    cookie_jars_[jar_id] = cookies;
    return true;
}

bool KSMWindowsHostServices::ClearCookies(
    const std::string& jar_id,
    const std::string& url
) {
    if (!IsHttpRequestUrlAllowed(url) || jar_id.empty()) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The URL origin is not allowlisted."
        );
    }
    std::lock_guard lock(capability_mutex_);
    cookie_jars_.erase(jar_id);
    return true;
}

std::optional<KSMFileScope> KSMWindowsHostServices::OpenFolder(
    const std::vector<std::string>& filters
) {
    (void)filters;
    Microsoft::WRL::ComPtr<IFileDialog> dialog;
    if (
        FAILED(CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog)
        )) || !dialog
    ) {
        return std::nullopt;
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    if (FAILED(dialog->Show(window_))) {
        return std::nullopt;
    }
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item)) || !item) {
        return std::nullopt;
    }
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || !path) {
        return std::nullopt;
    }
    const std::filesystem::path root(path);
    CoTaskMemFree(path);
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error) {
        return std::nullopt;
    }
    std::lock_guard lock(capability_mutex_);
    const std::string id = "scope-" +
        std::to_string(next_capability_id_++);
    scopes_.emplace(id, FileScopeState{root, {}});
    return KSMFileScope{
        id,
        "app://scope/" + id,
        {}
    };
}

std::optional<KSMFileScope> KSMWindowsHostServices::OpenFiles(
    const std::vector<std::string>& filters
) {
    (void)filters;
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (
        FAILED(CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog)
        )) || !dialog
    ) {
        return std::nullopt;
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM);
    }
    if (FAILED(dialog->Show(window_))) {
        return std::nullopt;
    }
    Microsoft::WRL::ComPtr<IShellItemArray> items;
    if (FAILED(dialog->GetResults(&items)) || !items) {
        return std::nullopt;
    }
    DWORD count = 0;
    items->GetCount(&count);
    std::vector<std::filesystem::path> paths;
    for (DWORD index = 0; index < count; ++index) {
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (FAILED(items->GetItemAt(index, &item)) || !item) {
            continue;
        }
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
            paths.emplace_back(path);
            CoTaskMemFree(path);
        }
    }
    if (paths.empty()) {
        return std::nullopt;
    }
    std::lock_guard lock(capability_mutex_);
    const std::string id = "scope-" +
        std::to_string(next_capability_id_++);
    const std::string root = "app://scope/" + id;
    std::vector<std::string> files;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        files.push_back(root + "/file-" + std::to_string(index));
    }
    scopes_.emplace(id, FileScopeState{paths.front().parent_path(), paths});
    return KSMFileScope{id, root, files};
}

std::optional<KSMWindowsHostServices::FileScopeState>
KSMWindowsHostServices::ResolveScope(const std::string& path) const {
    const std::string prefix = "app://scope/";
    if (!path.starts_with(prefix)) {
        return std::nullopt;
    }
    const std::string remainder = path.substr(prefix.size());
    const std::string id = remainder.substr(0, remainder.find('/'));
    std::lock_guard lock(capability_mutex_);
    const auto iterator = scopes_.find(id);
    if (iterator == scopes_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::optional<std::filesystem::path>
KSMWindowsHostServices::ResolveScopeFile(const std::string& path) const {
    const auto scope = ResolveScope(path);
    if (!scope) {
        return std::nullopt;
    }
    const std::string prefix = "app://scope/";
    const std::string remainder = path.substr(prefix.size());
    const auto slash = remainder.find('/');
    if (slash == std::string::npos) {
        return ResolveScopeTarget(scope->root, path);
    }
    const std::string relative = remainder.substr(slash + 1);
    if (relative.starts_with("file-")) {
        const std::string suffix = relative.substr(5);
        if (
            suffix.empty() ||
            !std::all_of(suffix.begin(), suffix.end(), [](unsigned char value) {
                return std::isdigit(value) != 0;
            })
        ) {
            return std::nullopt;
        }
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoull(suffix));
        } catch (const std::exception&) {
            return std::nullopt;
        }
        if (index >= scope->files.size()) {
            return std::nullopt;
        }
        return scope->files[index];
    }
    return ResolveScopeTarget(scope->root, path);
}

std::optional<std::vector<KSMWalkEntry>> KSMWindowsHostServices::Walk(
    const std::string& path,
    const std::string& request_id
) {
    (void)request_id;
    const auto scope = ResolveScope(path);
    if (!scope) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The path is outside an active file scope."
        );
    }
    std::vector<KSMWalkEntry> entries;
    std::error_code error;
    if (!scope->files.empty()) {
        for (std::size_t index = 0; index < scope->files.size(); ++index) {
            const auto& file = scope->files[index];
            const auto size = std::filesystem::file_size(file, error);
            if (error) {
                continue;
            }
            entries.push_back(KSMWalkEntry{
            path + "/file-" + std::to_string(index),
                reinterpret_cast<const char*>(file.filename().u8string().c_str()),
                false,
                size,
                std::nullopt
            });
        }
        return entries;
    }
    for (std::filesystem::recursive_directory_iterator iterator(
             scope->root,
             std::filesystem::directory_options::skip_permission_denied,
             error
         ), end;
         iterator != end && !error;
         iterator.increment(error)) {
        const auto& entry = *iterator;
        std::error_code status_error;
        const auto status = entry.symlink_status(status_error);
        if (status_error || std::filesystem::is_symlink(status)) {
            continue;
        }
        const auto relative = std::filesystem::relative(
            entry.path(),
            scope->root,
            status_error
        );
        if (status_error) {
            continue;
        }
        const auto relative_text = relative.generic_u8string();
        const std::string name(
            reinterpret_cast<const char*>(relative_text.data()),
            relative_text.size()
        );
        const bool directory = std::filesystem::is_directory(status);
        const std::uint64_t size = directory
            ? 0
            : std::filesystem::file_size(entry.path(), status_error);
        const auto filename = entry.path().filename().u8string();
        const std::string filename_text(
            reinterpret_cast<const char*>(filename.data()),
            filename.size()
        );
        entries.push_back(KSMWalkEntry{
            path + "/" + name,
            filename_text,
            directory,
            status_error ? 0 : size,
            std::nullopt
        });
    }
    return entries;
}

bool KSMWindowsHostServices::ReleaseScope(const std::string& id) {
    std::lock_guard lock(capability_mutex_);
    return scopes_.erase(id) > 0;
}

std::optional<KSMMediaProbe> KSMWindowsHostServices::ProbeMedia(
    const std::string& path
) {
    const auto resolved = ResolveScopeFile(path);
    if (!resolved || !std::filesystem::is_regular_file(*resolved)) {
        return std::nullopt;
    }
    const auto extension = LowerAscii(resolved->extension().string());
    if (extension != ".mp3" && extension != ".flac" &&
        extension != ".m4a" && extension != ".wav" &&
        extension != ".ogg" && extension != ".opus") {
        return KSMMediaProbe{};
    }
    KSMMediaProbe probe;
    probe.title = resolved->stem().string();
    probe.format = extension.substr(1);
    return probe;
}

std::optional<KSMExternalTool>
KSMWindowsHostServices::DetectExternalTool(const std::string& name) {
    if (name != "ffmpeg" && name != "ffprobe") {
        return KSMExternalTool{"", "", false};
    }
    std::wstring configured;
    {
        std::lock_guard lock(capability_mutex_);
        const auto iterator = external_tools_.find(name);
        if (iterator != external_tools_.end()) {
            configured = ToWide(iterator->second);
        }
    }
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = SearchPathW(
        nullptr,
        configured.empty() ? ToWide(name + ".exe").c_str() : configured.c_str(),
        nullptr,
        ARRAYSIZE(buffer),
        buffer,
        nullptr
    );
    if (length == 0 || length >= ARRAYSIZE(buffer)) {
        return KSMExternalTool{"", "", false};
    }
    return KSMExternalTool{ToUtf8(std::wstring(buffer, length)), "", true};
}

std::optional<KSMExternalTool>
KSMWindowsHostServices::ValidateExternalTool(const std::string& path) {
    const auto resolved = file_system_
        ? file_system_->ResolveReadableFile(path)
        : std::nullopt;
    if (resolved && std::filesystem::is_regular_file(*resolved)) {
        return KSMExternalTool{path, "", true};
    }
    return std::nullopt;
}

bool KSMWindowsHostServices::SetExternalToolPath(
    const std::string& name,
    const std::string& path
) {
    if (name.empty() || path.empty()) {
        return false;
    }
    std::lock_guard lock(capability_mutex_);
    external_tools_[name] = path;
    return true;
}

std::optional<std::string> KSMWindowsHostServices::PickExternalTool(
    const std::string& name
) {
    if (name != "ffmpeg" && name != "ffprobe") {
        return std::nullopt;
    }
    return OpenFile({".exe"});
}

std::string KSMWindowsHostServices::AppVersion() const {
    return app_version_;
}

std::string KSMWindowsHostServices::AppId() const {
    return app_id_;
}

std::string KSMWindowsHostServices::Language() const {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0) {
        return "en-US";
    }
    return ToUtf8(locale);
}

bool KSMWindowsHostServices::ShowMessage(
    const std::string& title,
    const std::string& message
) {
    return MessageBoxW(
        window_,
        ToWide(message).c_str(),
        ToWide(title).c_str(),
        MB_OK | MB_ICONINFORMATION
    ) == IDOK;
}

bool KSMWindowsHostServices::Confirm(
    const std::string& title,
    const std::string& message
) {
    return MessageBoxW(
        window_,
        ToWide(message).c_str(),
        ToWide(title).c_str(),
        MB_OKCANCEL | MB_ICONQUESTION
    ) == IDOK;
}

std::optional<std::string> KSMWindowsHostServices::OpenFile(
    const std::vector<std::string>& filters
) {
    std::unique_lock picker_lock(picker_mutex_, std::try_to_lock);
    if (!picker_lock.owns_lock()) {
        throw KSMHostException(
            "BUSY",
            "A file picker is already active."
        );
    }
    const std::wstring filter = BuildFilter(filters);
    std::vector<wchar_t> buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) {
        return std::nullopt;
    }
    return RegisterPicked(std::filesystem::path(buffer.data()));
}

std::optional<std::string> KSMWindowsHostServices::SaveFile(
    const std::string& suggested_name
) {
    std::unique_lock picker_lock(picker_mutex_, std::try_to_lock);
    if (!picker_lock.owns_lock()) {
        throw KSMHostException(
            "BUSY",
            "A file picker is already active."
        );
    }
    std::vector<wchar_t> buffer(32768, L'\0');
    const std::wstring initial_name = ToWide(suggested_name);
    if (!initial_name.empty()) {
        wcsncpy_s(buffer.data(), buffer.size(), initial_name.c_str(), _TRUNCATE);
    }
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"All Files\0*.*\0\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog)) {
        return std::nullopt;
    }
    return RegisterPicked(std::filesystem::path(buffer.data()));
}

std::optional<std::string> KSMWindowsHostServices::RegisterPicked(
    const std::filesystem::path& path
) {
    std::lock_guard lock(picked_mutex_);
    const std::string token = std::to_string(next_picked_id_++);
    const std::string virtual_path = KSMVirtualPath::PickedPath(token);
    picked_files_[virtual_path] = path;
    return virtual_path;
}

std::optional<std::filesystem::path> KSMWindowsHostServices::ResolvePicked(
    const std::string& path
) const {
    std::lock_guard lock(picked_mutex_);
    const auto iterator = picked_files_.find(path);
    if (iterator == picked_files_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::optional<std::string> KSMWindowsHostServices::ReadPickedText(
    const std::string& path
) {
    const auto resolved = ResolvePicked(path);
    if (!resolved || !std::filesystem::is_regular_file(*resolved)) {
        return std::nullopt;
    }
    std::error_code size_error;
    const auto size = std::filesystem::file_size(*resolved, size_error);
    if (size_error || size > KSMFileSystem::kMaxTextBytes) {
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

bool KSMWindowsHostServices::WritePickedText(
    const std::string& path,
    const std::string& content
) {
    if (content.size() > KSMFileSystem::kMaxTextBytes) {
        return false;
    }
    const auto resolved = ResolvePicked(path);
    if (!resolved) {
        return false;
    }
    std::ofstream output(*resolved, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output << content;
    return output.good();
}

bool KSMWindowsHostServices::PickedExists(const std::string& path) {
    const auto resolved = ResolvePicked(path);
    return resolved.has_value() && std::filesystem::exists(*resolved);
}

std::optional<KSMFileMetadata> KSMWindowsHostServices::PickedStat(
    const std::string& path
) {
    const auto resolved = ResolvePicked(path);
    if (!resolved) {
        return std::nullopt;
    }
    std::error_code error;
    const auto link_status = std::filesystem::symlink_status(*resolved, error);
    if (error || link_status.type() == std::filesystem::file_type::symlink) {
        return std::nullopt;
    }
    const auto status = std::filesystem::status(*resolved, error);
    if (error || !std::filesystem::exists(status)) {
        return std::nullopt;
    }
    std::optional<std::int64_t> modified_at;
    const auto timestamp = std::filesystem::last_write_time(*resolved, error);
    if (!error) {
        const auto system_timestamp =
            std::chrono::clock_cast<std::chrono::system_clock>(timestamp);
        modified_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            system_timestamp.time_since_epoch()
        ).count();
    }
    if (std::filesystem::is_directory(status)) {
        return KSMFileMetadata{KSMFileKind::Directory, 0, modified_at};
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::nullopt;
    }
    const auto size = std::filesystem::file_size(*resolved, error);
    if (error) {
        return std::nullopt;
    }
    return KSMFileMetadata{KSMFileKind::File, size, modified_at};
}

bool KSMWindowsHostServices::RemovePicked(const std::string& path) {
    std::filesystem::path resolved;
    {
        std::lock_guard lock(picked_mutex_);
        const auto iterator = picked_files_.find(path);
        if (iterator == picked_files_.end()) {
            return false;
        }
        resolved = iterator->second;
        picked_files_.erase(iterator);
    }
    std::error_code error;
    return std::filesystem::remove(resolved, error) && !error;
}

std::optional<std::string> KSMWindowsHostServices::ReadClipboardText() {
    if (!OpenClipboard(window_)) {
        return std::nullopt;
    }
    const HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return std::nullopt;
    }
    const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
    if (!text) {
        CloseClipboard();
        return std::nullopt;
    }
    const std::string result = ToUtf8(text);
    GlobalUnlock(data);
    CloseClipboard();
    return result;
}

bool KSMWindowsHostServices::WriteClipboardText(const std::string& text) {
    if (!OpenClipboard(window_)) {
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }
    const std::wstring wide_text = ToWide(text);
    const std::size_t bytes = (wide_text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    auto* target = static_cast<wchar_t*>(GlobalLock(memory));
    if (!target) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    std::memcpy(target, wide_text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

bool KSMWindowsHostServices::ShowNotification(
    const std::string& title,
    const std::string& body
) {
    if (!window_) {
        return false;
    }
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = 0x4B534D;
    data.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
    data.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
    wcsncpy_s(data.szTip, L"Kasumi", _TRUNCATE);
    wcsncpy_s(data.szInfoTitle, ToWide(title).c_str(), _TRUNCATE);
    wcsncpy_s(data.szInfo, ToWide(body).c_str(), _TRUNCATE);
    data.dwInfoFlags = NIIF_INFO;
    BOOL shown = Shell_NotifyIconW(NIM_ADD, &data);
    if (!shown) {
        shown = Shell_NotifyIconW(NIM_MODIFY, &data);
    }
    return shown != FALSE;
}

bool KSMWindowsHostServices::SetWindowTitle(const std::string& title) {
    return window_ && SetWindowTextW(window_, ToWide(title).c_str()) != FALSE;
}

bool KSMWindowsHostServices::MinimizeWindow() {
    if (!window_) {
        return false;
    }
    ShowWindow(window_, SW_MINIMIZE);
    return true;
}

bool KSMWindowsHostServices::MaximizeWindow() {
    if (!window_) {
        return false;
    }
    ShowWindow(window_, SW_MAXIMIZE);
    return true;
}

bool KSMWindowsHostServices::CloseWindow() {
    return window_ && PostMessageW(window_, WM_CLOSE, 0, 0) != FALSE;
}

bool KSMWindowsHostServices::ExitApp() {
    return window_ && PostMessageW(window_, WM_CLOSE, 0, 0) != FALSE;
}

void KSMWindowsHostServices::CloseDownloadHandles(
    const std::shared_ptr<DownloadState>& state
) {
    std::lock_guard lock(state->mutex);
    if (state->request) {
        WinHttpCloseHandle(state->request);
        state->request = nullptr;
    }
    if (state->connect) {
        WinHttpCloseHandle(state->connect);
        state->connect = nullptr;
    }
    if (state->session) {
        WinHttpCloseHandle(state->session);
        state->session = nullptr;
    }
}

void KSMWindowsHostServices::SetDownloadHandles(
    const std::shared_ptr<DownloadState>& state,
    HINTERNET session,
    HINTERNET connect,
    HINTERNET request
) {
    std::lock_guard lock(state->mutex);
    if (state->request) {
        WinHttpCloseHandle(state->request);
    }
    if (state->connect) {
        WinHttpCloseHandle(state->connect);
    }
    if (state->session) {
        WinHttpCloseHandle(state->session);
    }
    state->session = session;
    state->connect = connect;
    state->request = request;
}

bool KSMWindowsHostServices::IsDownloadCancelled(
    const std::shared_ptr<DownloadState>& state
) const {
    return state->cancelled.load();
}

std::optional<KSMHttpDownloadResult> KSMWindowsHostServices::Download(
    const std::string& url,
    const std::string& path,
    bool overwrite,
    const std::string& request_id
) {
    if (!file_system_) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The download service is not configured."
        );
    }
    const std::string key = request_id.empty()
        ? "anonymous-" + std::to_string(next_download_part.fetch_add(1))
        : request_id;
    auto state = std::make_shared<DownloadState>();
    {
        std::lock_guard lock(download_mutex_);
        if (downloads_.contains(key)) {
            throw KSMHostException(
                "BUSY",
                "A download with this request ID is already running."
            );
        }
        downloads_.emplace(key, state);
    }
    const auto release_reservation = [this, &key, &state]() {
        std::lock_guard lock(download_mutex_);
        const auto iterator = downloads_.find(key);
        if (iterator != downloads_.end() && iterator->second == state) {
            downloads_.erase(iterator);
        }
    };

    std::optional<std::filesystem::path> target;
    try {
#if defined(KASUMI_ENABLE_TEST_HOOKS)
        std::function<void()> test_gate;
        {
            std::lock_guard lock(download_test_gate_mutex_);
            test_gate = download_test_gate_;
        }
        if (test_gate) {
            test_gate();
        }
#endif
        if (!KSMUrlPolicy::ParseHttpUrl(url, true)) {
            throw KSMHostException("INVALID_ARGS", "url must use HTTPS.");
        }
        if (!KSMUrlPolicy::IsAllowedDownloadUrl(
                url,
                allowed_http_origins_
            )) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted for downloads."
            );
        }
        const auto initial_url = KSMUrlPolicy::ParseHttpUrl(url, true);
        if (!initial_url || !IsPublicHost(initial_url->origin.host)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The download host is not a public network address."
            );
        }

        target = file_system_->ResolveWritableFile(path);
        if (!target) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "path is outside the writable sandbox."
            );
        }
        if (IsDirectory(*target)) {
            throw KSMHostException("INVALID_ARGS", "path must name a file.");
        }
        if (std::filesystem::exists(*target) && !overwrite) {
            throw KSMHostException("IO_ERROR", "The target already exists.");
        }

        std::error_code directory_error;
        std::filesystem::create_directories(
            target->parent_path(),
            directory_error
        );
        if (directory_error) {
            throw KSMHostException(
                "IO_ERROR",
                "The target directory could not be created."
            );
        }
        target = file_system_->ResolveWritableFile(path);
        if (!target) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "path is outside the writable sandbox."
            );
        }
    } catch (...) {
        release_reservation();
        throw;
    }

    const std::filesystem::path part = target->parent_path() /
        (L"." + target->filename().wstring() + L"." +
            std::to_wstring(next_download_part.fetch_add(1)) + L".part");
    std::ofstream output(part, std::ios::binary | std::ios::trunc);
    if (!output) {
        {
            std::lock_guard lock(download_mutex_);
            downloads_.erase(key);
        }
        throw KSMHostException("IO_ERROR", "The temporary file could not be created.");
    }

    const auto cleanup = [this, &key, &part, &state]() {
        CloseDownloadHandles(state);
        {
            std::lock_guard lock(download_mutex_);
            const auto iterator = downloads_.find(key);
            if (iterator != downloads_.end() && iterator->second == state) {
                downloads_.erase(iterator);
            }
        }
        std::error_code error;
        std::filesystem::remove(part, error);
    };

    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(kDownloadTimeoutMs);
    std::string current_url = url;
    std::uint64_t total_bytes = 0;

    try {
        for (int redirects = 0; ; ++redirects) {
            if (IsDownloadCancelled(state)) {
                throw KSMHostException("CANCELLED", "The download was cancelled.");
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                throw KSMHostException("TIMEOUT", "The download timed out.");
            }

            const auto parsed = KSMUrlPolicy::ParseHttpUrl(current_url, true);
            if (!parsed) {
                throw KSMHostException("IO_ERROR", "The redirect target is invalid.");
            }
            if (!KSMUrlPolicy::IsAllowedDownloadUrl(
                    current_url,
                    allowed_http_origins_
                )) {
                throw KSMHostException(
                    "PERMISSION_DENIED",
                    "The redirect origin is not allowlisted for downloads."
                );
            }
            if (!IsPublicHost(parsed->origin.host)) {
                throw KSMHostException(
                    "PERMISSION_DENIED",
                    "The download host is not a public network address."
                );
            }
            HINTERNET session = WinHttpOpen(
                L"Kasumi/0.1",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if (!session) {
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Opening the HTTP session")
                );
            }
            if (!WinHttpSetTimeouts(
                session,
                kDownloadIoTimeoutMs,
                kDownloadIoTimeoutMs,
                kDownloadIoTimeoutMs,
                kDownloadIoTimeoutMs
            )) {
                SetDownloadHandles(state, session, nullptr, nullptr);
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Configuring HTTP timeouts")
                );
            }

            const std::wstring host = ToWide(parsed->origin.host);
            HINTERNET connect = WinHttpConnect(
                session,
                host.c_str(),
                static_cast<INTERNET_PORT>(parsed->origin.port),
                0
            );
            if (!connect) {
                SetDownloadHandles(state, session, nullptr, nullptr);
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Connecting to the server")
                );
            }

            const std::wstring request_path = ToWide(
                parsed->path + parsed->query
            );
            HINTERNET request = WinHttpOpenRequest(
                connect,
                L"GET",
                request_path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE
            );
            if (!request) {
                SetDownloadHandles(state, session, connect, nullptr);
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Opening the HTTP request")
                );
            }
            SetDownloadHandles(state, session, connect, request);
            if (IsDownloadCancelled(state)) {
                throw KSMHostException("CANCELLED", "The download was cancelled.");
            }

            DWORD disable_redirects = WINHTTP_DISABLE_REDIRECTS;
            if (!WinHttpSetOption(
                request,
                WINHTTP_OPTION_DISABLE_FEATURE,
                &disable_redirects,
                sizeof(disable_redirects)
            )) {
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Disabling automatic HTTP redirects")
                );
            }
            if (!WinHttpSendRequest(
                request,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0
            )) {
                if (IsDownloadCancelled(state)) {
                    throw KSMHostException("CANCELLED", "The download was cancelled.");
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    throw KSMHostException("TIMEOUT", "The download timed out.");
                }
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Sending the HTTP request")
                );
            }
            if (!WinHttpReceiveResponse(request, nullptr)) {
                if (IsDownloadCancelled(state)) {
                    throw KSMHostException("CANCELLED", "The download was cancelled.");
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    throw KSMHostException("TIMEOUT", "The download timed out.");
                }
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Receiving the HTTP response")
                );
            }

            DWORD status = 0;
            DWORD status_size = sizeof(status);
            if (!WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &status_size,
                WINHTTP_NO_HEADER_INDEX
            )) {
                throw KSMHostException(
                    "IO_ERROR",
                    HttpErrorText("Reading the HTTP status")
                );
            }

            if (status >= 300 && status < 400) {
                if (redirects >= kMaxRedirects) {
                    throw KSMHostException(
                        "IO_ERROR",
                        "The download exceeded the redirect limit."
                    );
                }
                const auto location = QueryRequestHeader(
                    request,
                    WINHTTP_QUERY_LOCATION
                );
                if (!location) {
                    throw KSMHostException(
                        "IO_ERROR",
                        "The redirect did not include a location."
                    );
                }
                const auto next = KSMUrlPolicy::ResolveHttpsRedirect(
                    current_url,
                    ToUtf8(*location)
                );
                if (!next) {
                    throw KSMHostException(
                        "IO_ERROR",
                        "The redirect did not use HTTPS."
                    );
                }
                const auto next_url = KSMUrlPolicy::ParseHttpUrl(*next, true);
                if (
                    !next_url ||
                    !KSMUrlPolicy::IsAllowedDownloadUrl(
                        *next,
                        allowed_http_origins_
                    ) ||
                    !IsPublicHost(next_url->origin.host)
                ) {
                    throw KSMHostException(
                        "PERMISSION_DENIED",
                        "The redirect target is not allowlisted for downloads."
                    );
                }
                CloseDownloadHandles(state);
                current_url = *next;
                continue;
            }
            if (status < 200 || status >= 300) {
                throw KSMHostException(
                    "IO_ERROR",
                    "The server returned HTTP " + std::to_string(status) + "."
                );
            }

            DWORD declared_length = 0;
            DWORD declared_size = sizeof(declared_length);
            if (WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &declared_length,
                &declared_size,
                WINHTTP_NO_HEADER_INDEX
            ) && declared_length > kMaxDownloadBytes) {
                throw KSMHostException(
                    "IO_ERROR",
                    "The download exceeds the maximum size."
                );
            }

            std::vector<unsigned char> buffer(kDownloadBufferBytes);
            while (true) {
                if (IsDownloadCancelled(state)) {
                    throw KSMHostException("CANCELLED", "The download was cancelled.");
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    throw KSMHostException("TIMEOUT", "The download timed out.");
                }
                DWORD read = 0;
                if (!WinHttpReadData(
                    request,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()),
                    &read
                )) {
                    if (IsDownloadCancelled(state)) {
                        throw KSMHostException("CANCELLED", "The download was cancelled.");
                    }
                    if (std::chrono::steady_clock::now() >= deadline) {
                        throw KSMHostException("TIMEOUT", "The download timed out.");
                    }
                    throw KSMHostException(
                        "IO_ERROR",
                        HttpErrorText("Reading the HTTP response body")
                    );
                }
                if (read == 0) {
                    break;
                }
                total_bytes += read;
                if (total_bytes > kMaxDownloadBytes) {
                    throw KSMHostException(
                        "IO_ERROR",
                        "The download exceeds the maximum size."
                    );
                }
                output.write(
                    reinterpret_cast<const char*>(buffer.data()),
                    static_cast<std::streamsize>(read)
                );
                if (!output.good()) {
                    throw KSMHostException(
                        "IO_ERROR",
                        "The downloaded file could not be written."
                    );
                }
            }

            output.flush();
            if (!output.good()) {
                throw KSMHostException(
                    "IO_ERROR",
                    "The downloaded file could not be written."
                );
            }
            output.close();
            CloseDownloadHandles(state);

            if (std::filesystem::exists(*target) && !overwrite) {
                throw KSMHostException("IO_ERROR", "The target already exists.");
            }
            DWORD move_flags = MOVEFILE_WRITE_THROUGH;
            if (overwrite) {
                move_flags |= MOVEFILE_REPLACE_EXISTING;
            }
            if (!MoveFileExW(part.c_str(), target->c_str(), move_flags)) {
                throw KSMHostException(
                    "IO_ERROR",
                    "The downloaded file could not be committed."
                );
            }

            {
                std::lock_guard lock(download_mutex_);
                const auto iterator = downloads_.find(key);
                if (iterator != downloads_.end() && iterator->second == state) {
                    downloads_.erase(iterator);
                }
            }
            return KSMHttpDownloadResult{
                path,
                total_bytes
            };
        }
    } catch (...) {
        output.close();
        cleanup();
        throw;
    }
}

bool KSMWindowsHostServices::IsExternalUrlAllowed(
    const std::string& url
) const {
    return KSMUrlPolicy::IsAllowedExternalUrl(
        url,
        allowed_external_origins_
    );
}

bool KSMWindowsHostServices::IsHttpRequestUrlAllowed(
    const std::string& url
) const {
    return KSMUrlPolicy::IsAllowedDownloadUrl(url, allowed_http_origins_);
}

bool KSMWindowsHostServices::OpenExternal(const std::string& url) {
    if (!KSMUrlPolicy::IsAllowedExternalUrl(url, allowed_external_origins_)) {
        throw KSMHostException(
            "PERMISSION_DENIED",
            "The URL origin is not allowlisted."
        );
    }
    const std::wstring wide_url = ToWide(url);
    const HINSTANCE result = ShellExecuteW(
        window_,
        L"open",
        wide_url.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool KSMWindowsHostServices::OpenPath(const std::string& path) {
    const std::wstring wide_path = ToWide(path);
    const HINSTANCE result = ShellExecuteW(
        window_,
        L"open",
        wide_path.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool KSMWindowsHostServices::ShowInFolder(const std::string& path) {
    const std::wstring wide_path = ToWide(path);
    const HINSTANCE result = ShellExecuteW(
        window_,
        L"open",
        L"explorer.exe",
        (L"/select,\"" + wide_path + L"\"").c_str(),
        nullptr,
        SW_SHOWNORMAL
    );
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool KSMWindowsHostServices::IsPrimaryInstance() const {
    std::lock_guard lock(capability_mutex_);
    return single_instance_mutex_ == nullptr;
}

bool KSMWindowsHostServices::FocusMainWindow() {
    if (!window_) {
        return false;
    }
    if (IsIconic(window_)) {
        ShowWindow(window_, SW_RESTORE);
    }
    return SetForegroundWindow(window_) != FALSE;
}

bool KSMWindowsHostServices::ConfigureTray(const KSMTrayConfig& config) {
    if (!window_) {
        return false;
    }
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon.uCallbackMessage = kTrayCallbackMessage;
    icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    const std::wstring tooltip = config.tooltip
        ? ToWide(*config.tooltip)
        : L"Kasumi";
    wcsncpy_s(icon.szTip, tooltip.c_str(), _TRUNCATE);
    {
        std::lock_guard lock(capability_mutex_);
        tray_items_ = config.items;
        if (tray_visible_) {
            Shell_NotifyIconW(NIM_DELETE, &icon);
        }
        tray_visible_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
        if (tray_visible_) {
            return true;
        }
        tray_items_.clear();
        return false;
    }
}

bool KSMWindowsHostServices::TrayShow() {
    if (!window_) {
        return false;
    }
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon.uCallbackMessage = kTrayCallbackMessage;
    icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(icon.szTip, L"Kasumi");
    std::lock_guard lock(capability_mutex_);
    if (tray_visible_) {
        return true;
    }
    tray_visible_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
    return tray_visible_;
}

bool KSMWindowsHostServices::TrayHide() {
    if (!window_) {
        return false;
    }
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    std::lock_guard lock(capability_mutex_);
    if (!tray_visible_) {
        return false;
    }
    tray_visible_ = !Shell_NotifyIconW(NIM_DELETE, &icon);
    return !tray_visible_;
}

bool KSMWindowsHostServices::RegisterGlobalShortcut(
    const std::string& shortcut
) {
    const auto parsed = ParseHotkey(shortcut);
    if (!parsed || !window_) {
        return false;
    }
    std::lock_guard lock(capability_mutex_);
    if (hotkeys_.contains(shortcut)) {
        return false;
    }
    int hotkey_id = static_cast<int>(kFirstHotkeyId + hotkey_ids_.size());
    const UINT key = static_cast<UINT>(
        static_cast<unsigned char>(parsed->second[0])
    );
    if (!RegisterHotKey(window_, hotkey_id, parsed->first, key)) {
        return false;
    }
    hotkeys_.insert(shortcut);
    hotkey_ids_[hotkey_id] = shortcut;
    return true;
}

bool KSMWindowsHostServices::UnregisterGlobalShortcut(
    const std::string& shortcut
) {
    std::lock_guard lock(capability_mutex_);
    const auto iterator = std::find_if(
        hotkey_ids_.begin(),
        hotkey_ids_.end(),
        [&shortcut](const auto& entry) {
            return entry.second == shortcut;
        }
    );
    if (iterator == hotkey_ids_.end() ||
        !UnregisterHotKey(window_, iterator->first)) {
        return false;
    }
    hotkey_ids_.erase(iterator);
    hotkeys_.erase(shortcut);
    return true;
}

bool KSMWindowsHostServices::InstallUpdate(
    const std::string& package_path,
    const std::string& expected_hash,
    const std::string& signature,
    const std::string& request_id
) {
    (void)expected_hash;
    (void)signature;
    (void)request_id;
    const auto resolved = file_system_
        ? file_system_->ResolveReadableFile(package_path)
        : std::nullopt;
    if (!resolved || !std::filesystem::is_regular_file(*resolved)) {
        return false;
    }
    return false;
}

std::optional<std::string>
KSMWindowsHostServices::TakePendingDeepLink() {
    std::lock_guard lock(capability_mutex_);
    if (pending_deep_links_.empty()) {
        return std::nullopt;
    }
    const std::string value = pending_deep_links_.front();
    pending_deep_links_.pop_front();
    return value;
}

bool KSMWindowsHostServices::OpenDevtools(bool enabled) {
    if (!window_) {
        return false;
    }
    std::lock_guard lock(capability_mutex_);
    devtools_open_ = enabled;
    return false;
}

bool KSMWindowsHostServices::DispatchSystemMessage(
    unsigned int message,
    std::uintptr_t wparam
) {
    if (message == WM_HOTKEY) {
        std::string shortcut;
        {
            std::lock_guard lock(capability_mutex_);
            const auto iterator = hotkey_ids_.find(static_cast<int>(wparam));
            if (iterator != hotkey_ids_.end()) {
                shortcut = iterator->second;
            }
        }
        if (!shortcut.empty()) {
            DispatchCapabilityEvent(
                "globalShortcut.triggered",
                nlohmann::json{{"shortcut", shortcut}}.dump()
            );
            return true;
        }
    }
    if (message == kTrayCallbackMessage) {
        const UINT event = LOWORD(static_cast<DWORD>(wparam));
        if (event == WM_LBUTTONUP || event == WM_RBUTTONUP) {
            DispatchCapabilityEvent(
                "tray.click",
                nlohmann::json{{"button", event == WM_LBUTTONUP
                    ? "left"
                    : "right"}}.dump()
            );
            return true;
        }
    }
    return false;
}

std::optional<std::vector<KSMAudioDevice>>
KSMWindowsHostServices::AudioDevices() const {
    return std::nullopt;
}

bool KSMWindowsHostServices::SetAudioDevice(const std::string& id) {
    (void)id;
    return false;
}

bool KSMWindowsHostServices::SetExclusiveAudioOutput(bool exclusive) {
    (void)exclusive;
    return false;
}

bool KSMWindowsHostServices::SetAudioEqualizer(
    const std::vector<double>& gains
) {
    (void)gains;
    return false;
}

bool KSMWindowsHostServices::SetAudioSpeed(double speed) {
    if (!std::isfinite(speed) || speed < 0.25 || speed > 4.0) {
        throw KSMHostException(
            "INVALID_ARGS",
            "speed must be between 0.25 and 4."
        );
    }
    std::lock_guard lock(audio_mutex_);
    audio_speed_ = speed;
    return !audio_loaded_ || MciCommand(
        L"set kasumi_audio speed " +
        std::to_wstring(static_cast<long long>(speed * 1000.0))
    );
}

bool KSMWindowsHostServices::SetAudioVolume(double volume) {
    if (!std::isfinite(volume) || volume < 0.0 || volume > 1.0) {
        throw KSMHostException(
            "INVALID_ARGS",
            "volume must be between 0 and 1."
        );
    }
    std::lock_guard lock(audio_mutex_);
    audio_volume_ = volume;
    return !audio_loaded_ || MciCommand(
        L"setaudio kasumi_audio volume to " +
        std::to_wstring(static_cast<long long>(volume * 1000.0))
    );
}

std::optional<KSMAudioProbe> KSMWindowsHostServices::ProbeAudio(
    const std::string& path
) {
    const auto probe = ProbeMedia(path);
    if (!probe) {
        return std::nullopt;
    }
    KSMAudioProbe result;
    result.duration_ms = probe->duration_ms;
    result.codec = probe->format;
    return result;
}

bool KSMWindowsHostServices::LoadAudio(const std::string& path) {
    const auto resolved = ResolveScopeFile(path);
    if (!resolved || !std::filesystem::is_regular_file(*resolved)) {
        throw KSMHostException(
            "NOT_FOUND",
            "The audio source does not exist."
        );
    }
    const auto wide_path = resolved->wstring();
    if (wide_path.find(L'"') != std::wstring::npos) {
        throw KSMHostException(
            "INVALID_ARGS",
            "The audio source contains an invalid quote."
        );
    }
    std::lock_guard lock(audio_mutex_);
    MciCommand(L"close kasumi_audio");
    if (!MciCommand(
            L"open \"" + wide_path +
            L"\" type mpegvideo alias kasumi_audio"
        )) {
        audio_loaded_ = false;
        audio_source_.clear();
        audio_duration_ms_.reset();
        throw KSMHostException(
            "MEDIA_UNSUPPORTED",
            "The audio source could not be opened."
        );
    }
    audio_source_ = path;
    audio_duration_ms_ = MciStatusMilliseconds(L"length");
    audio_loaded_ = true;
    audio_playing_ = false;
    MciCommand(
        L"setaudio kasumi_audio volume to " +
        std::to_wstring(static_cast<long long>(audio_volume_ * 1000.0))
    );
    MciCommand(
        L"set kasumi_audio speed " +
        std::to_wstring(static_cast<long long>(audio_speed_ * 1000.0))
    );
    return true;
}

bool KSMWindowsHostServices::SetAudioPlayback(bool playing) {
    std::lock_guard lock(audio_mutex_);
    if (!audio_loaded_) {
        return false;
    }
    const bool updated = playing
        ? MciCommand(L"play kasumi_audio")
        : MciCommand(L"pause kasumi_audio");
    if (updated) {
        audio_playing_ = playing;
    }
    return updated;
}

bool KSMWindowsHostServices::SeekAudio(std::int64_t position_ms) {
    if (position_ms < 0) {
        throw KSMHostException(
            "INVALID_ARGS",
            "positionMs must be non-negative."
        );
    }
    std::lock_guard lock(audio_mutex_);
    if (!audio_loaded_) {
        return false;
    }
    const bool updated = MciCommand(
        L"seek kasumi_audio to " + std::to_wstring(position_ms)
    );
    if (updated && audio_playing_) {
        MciCommand(L"play kasumi_audio");
    }
    return updated;
}

std::optional<KSMAudioState> KSMWindowsHostServices::GetAudioState() const {
    std::lock_guard lock(audio_mutex_);
    KSMAudioState state;
    state.playing = audio_playing_;
    state.playback = audio_loaded_
        ? (audio_playing_ ? "playing" : "paused")
        : "idle";
    state.position_ms = audio_loaded_
        ? MciStatusMilliseconds(L"position").value_or(0)
        : 0;
    state.duration_ms = audio_duration_ms_;
    state.volume = audio_volume_;
    state.speed = audio_speed_;
    state.source = audio_source_.empty()
        ? std::nullopt
        : std::optional<std::string>(audio_source_);
    return state;
}

bool KSMWindowsHostServices::UpdateMediaSession(
    const KSMMediaSessionMetadata& metadata
) {
    std::lock_guard lock(audio_mutex_);
    if (!audio_loaded_) {
        return false;
    }
    if (metadata.position_ms) {
        const bool updated = MciCommand(
            L"seek kasumi_audio to " +
            std::to_wstring(std::max<std::int64_t>(0, *metadata.position_ms))
        );
        if (!updated) {
            return false;
        }
    }
    if (metadata.state == "playing") {
        if (!MciCommand(L"play kasumi_audio")) {
            return false;
        }
        audio_playing_ = true;
    } else if (metadata.state == "paused") {
        const bool updated = MciCommand(L"pause kasumi_audio");
        audio_playing_ = false;
        if (!updated) {
            return false;
        }
    } else if (metadata.state == "stopped") {
        const bool updated = MciCommand(L"stop kasumi_audio");
        audio_playing_ = false;
        if (!updated) {
            return false;
        }
        if (!MciCommand(L"seek kasumi_audio to 0")) {
            return false;
        }
    }
    return true;
}

bool KSMWindowsHostServices::ClearMediaSession() {
    std::lock_guard lock(audio_mutex_);
    if (!audio_loaded_) {
        return true;
    }
    audio_playing_ = false;
    return MciCommand(L"pause kasumi_audio");
}

std::optional<std::string> KSMWindowsHostServices::CreateWindow(
    const std::string& route,
    const KSMWindowState& state
) {
    (void)route;
    (void)state;
    return std::nullopt;
}

bool KSMWindowsHostServices::ShowWindowById(
    const std::string& window_id,
    bool visible
) {
    (void)window_id;
    if (!window_) {
        return false;
    }
    return ShowWindow(window_, visible ? SW_SHOW : SW_HIDE) != FALSE;
}

bool KSMWindowsHostServices::SetWindowPosition(
    const std::string& window_id,
    int x,
    int y
) {
    (void)window_id;
    if (!window_) {
        return false;
    }
    return SetWindowPos(
        window_,
        nullptr,
        x,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    ) != FALSE;
}

bool KSMWindowsHostServices::SetWindowAlwaysOnTop(
    const std::string& window_id,
    bool enabled
) {
    (void)window_id;
    if (!window_) {
        return false;
    }
    return SetWindowPos(
        window_,
        enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    ) != FALSE;
}

bool KSMWindowsHostServices::SetWindowTransparent(
    const std::string& window_id,
    bool enabled
) {
    (void)window_id;
    if (!window_) {
        return false;
    }
    LONG style = GetWindowLongW(window_, GWL_EXSTYLE);
    if (enabled) {
        style |= WS_EX_LAYERED;
        SetWindowLongW(window_, GWL_EXSTYLE, style);
        return SetLayeredWindowAttributes(window_, 0, 220, LWA_ALPHA)
            != FALSE;
    }
    style &= ~WS_EX_LAYERED;
    SetWindowLongW(window_, GWL_EXSTYLE, style);
    return SetWindowPos(
        window_,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOACTIVATE | SWP_FRAMECHANGED
    ) != FALSE;
}

std::optional<KSMWindowState> KSMWindowsHostServices::GetWindowState(
    const std::string& window_id
) const {
    (void)window_id;
    if (!window_) {
        return std::nullopt;
    }
    RECT rect{};
    if (!GetWindowRect(window_, &rect)) {
        return std::nullopt;
    }
    const LONG style = GetWindowLongW(window_, GWL_EXSTYLE);
    KSMWindowState state;
    state.visible = IsWindowVisible(window_) != FALSE;
    state.always_on_top = (style & WS_EX_TOPMOST) != 0;
    state.transparent = (style & WS_EX_LAYERED) != 0;
    state.x = rect.left;
    state.y = rect.top;
    state.width = rect.right - rect.left;
    state.height = rect.bottom - rect.top;
    return state;
}

bool KSMWindowsHostServices::EmitWindowEvent(
    const std::string& window_id,
    const std::string& event,
    const std::string& payload
) {
    (void)window_id;
    DispatchCapabilityEvent(event, payload);
    std::lock_guard lock(capability_mutex_);
    return static_cast<bool>(event_dispatcher_);
}

bool KSMWindowsHostServices::CheckUpdate(
    bool& available,
    std::string& version
) {
    available = false;
    version.clear();
    return true;
}

bool KSMWindowsHostServices::DownloadUpdate(const std::string& version) {
    (void)version;
    return false;
}

bool KSMWindowsHostServices::SetWindowSize(const KSMWindowSize& size) {
    if (!window_) {
        return false;
    }
    if (
        IsIconic(window_) ||
        IsZoomed(window_) ||
        IsFullscreenWindow(window_)
    ) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The window cannot be resized in its current state."
        );
    }
    if (
        size.width < kMinWindowWidth ||
        size.height < kMinWindowHeight
    ) {
        throw KSMHostException(
            "INVALID_ARGS",
            "width must be at least 320 and height at least 240."
        );
    }

    UINT dpi = GetDpiForWindow(window_);
    if (dpi == 0) {
        dpi = GetDpiForSystem();
    }
    if (dpi == 0) {
        dpi = 96;
    }
    int width = MulDiv(size.width, static_cast<int>(dpi), 96);
    int height = MulDiv(size.height, static_cast<int>(dpi), 96);

    const HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return false;
    }
    const int work_width = info.rcWork.right - info.rcWork.left;
    const int work_height = info.rcWork.bottom - info.rcWork.top;
    if (work_width < kMinWindowWidth || work_height < kMinWindowHeight) {
        return false;
    }
    width = std::clamp(width, kMinWindowWidth, work_width);
    height = std::clamp(height, kMinWindowHeight, work_height);

    RECT current{};
    if (!GetWindowRect(window_, &current)) {
        return false;
    }
    const int x = std::clamp(
        static_cast<int>(current.left),
        static_cast<int>(info.rcWork.left),
        static_cast<int>(info.rcWork.right) - width
    );
    const int y = std::clamp(
        static_cast<int>(current.top),
        static_cast<int>(info.rcWork.top),
        static_cast<int>(info.rcWork.bottom) - height
    );
    return SetWindowPos(
        window_,
        nullptr,
        x,
        y,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE
    ) != FALSE;
}

std::optional<KSMDeviceInfo> KSMWindowsHostServices::GetDeviceInfo() const {
    return std::nullopt;
}

std::optional<KSMOperatingSystemInfo>
KSMWindowsHostServices::GetOperatingSystemInfo() const {
    RTL_OSVERSIONINFOW version_info{};
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return std::nullopt;
    }
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion")
    );
    if (!rtl_get_version || rtl_get_version(&version_info) != 0) {
        return std::nullopt;
    }

    SYSTEM_INFO system_info{};
    GetNativeSystemInfo(&system_info);
    std::string architecture = "unknown";
    switch (system_info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        architecture = "x86_64";
        break;
    case PROCESSOR_ARCHITECTURE_ARM64:
        architecture = "arm64";
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        architecture = "arm";
        break;
    case PROCESSOR_ARCHITECTURE_INTEL:
        architecture = "x86";
        break;
    default:
        break;
    }

    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0) {
        return std::nullopt;
    }

    DYNAMIC_TIME_ZONE_INFORMATION timezone{};
    std::string timezone_id;
    if (GetDynamicTimeZoneInformation(&timezone) != TIME_ZONE_ID_INVALID) {
        timezone_id = ToUtf8(timezone.TimeZoneKeyName);
        if (timezone_id.empty()) {
            timezone_id = ToUtf8(timezone.StandardName);
        }
    }
    if (timezone_id.empty()) {
        timezone_id = "UTC";
    }

    return KSMOperatingSystemInfo{
        "windows",
        architecture,
        std::to_string(version_info.dwMajorVersion) + "." +
            std::to_string(version_info.dwMinorVersion) + "." +
            std::to_string(version_info.dwBuildNumber),
        ToUtf8(locale),
        timezone_id,
        false,
        std::nullopt,
        std::nullopt
    };
}

bool KSMWindowsHostServices::WriteLog(
    const std::string& level,
    const std::string& target,
    const std::string& message
) {
    const std::string line =
        "[" + level + "] [" +
        (target.empty() ? std::string("kasumi") : target) +
        "] " + message + "\n";
    OutputDebugStringW(ToWide(line).c_str());
    return true;
}

void KSMWindowsHostServices::CancelRequest(const std::string& request_id) {
    KSMHostServices::CancelRequest(request_id);
    std::shared_ptr<DownloadState> state;
    {
        std::lock_guard lock(download_mutex_);
        const auto iterator = downloads_.find(request_id);
        if (iterator != downloads_.end()) {
            state = iterator->second;
        }
    }
    if (!state) {
        return;
    }
    state->cancelled.store(true);
    CloseDownloadHandles(state);
    std::shared_ptr<TransferState> transfer;
    {
        std::lock_guard lock(capability_mutex_);
        const auto iterator = transfers_.find(request_id);
        if (iterator != transfers_.end()) {
            transfer = iterator->second;
        }
    }
    if (transfer) {
        transfer->cancelled.store(true);
        CloseTransferHandles(transfer);
    }
}

void KSMWindowsHostServices::CancelRequestWithScope(
    const std::string& request_id
) {
    CancelRequest(request_id);
}
