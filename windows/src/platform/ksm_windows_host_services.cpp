#include "ksm_windows_host_services.h"

#include <commdlg.h>
#include <shellapi.h>
#include <winhttp.h>
#include <winternl.h>
#include <windns.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <system_error>
#include <vector>

#include "../commands/ksm_filesystem.h"
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

KSMWindowsHostServices::~KSMWindowsHostServices() = default;

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
    return true;
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

bool KSMWindowsHostServices::TrayShow() {
    return window_ != nullptr;
}

bool KSMWindowsHostServices::TrayHide() {
    return window_ != nullptr;
}

bool KSMWindowsHostServices::RegisterGlobalShortcut(
    const std::string& shortcut
) {
    (void)shortcut;
    return false;
}

bool KSMWindowsHostServices::UnregisterGlobalShortcut(
    const std::string& shortcut
) {
    (void)shortcut;
    return false;
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
}
