#include "ksm_windows_host_services.h"

#include <commdlg.h>
#include <shellapi.h>
#include <winhttp.h>

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
#include <utility>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "../platform/ksm_virtual_path.h"

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
    std::shared_ptr<KSMFileSystem> file_system
)
    : window_(window),
      file_system_(std::move(file_system)),
      allowed_external_origins_(
          KSMUrlPolicy::NormalizeOrigins(allowed_external_origins)
      ) {
}

std::string KSMWindowsHostServices::AppVersion() const {
    return "0.1.0";
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
    if (!KSMUrlPolicy::ParseHttpUrl(url, true)) {
        throw KSMHostException("INVALID_ARGS", "url must use HTTPS.");
    }

    auto target = file_system_->ResolveWritableFile(path);
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
    std::filesystem::create_directories(target->parent_path(), directory_error);
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

    const std::filesystem::path part = target->parent_path() /
        (L"." + target->filename().wstring() + L"." +
            std::to_wstring(next_download_part.fetch_add(1)) + L".part");
    std::ofstream output(part, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw KSMHostException("IO_ERROR", "The temporary file could not be created.");
    }

    const std::string key = request_id.empty()
        ? "anonymous-" + std::to_string(next_download_part.fetch_add(1))
        : request_id;
    auto state = std::make_shared<DownloadState>();
    {
        std::lock_guard lock(download_mutex_);
        downloads_[key] = state;
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
            WinHttpSetTimeouts(
                session,
                kDownloadIoTimeoutMs,
                kDownloadIoTimeoutMs,
                kDownloadIoTimeoutMs,
                kDownloadIoTimeoutMs
            );

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
            WinHttpSetOption(
                request,
                WINHTTP_OPTION_DISABLE_FEATURE,
                &disable_redirects,
                sizeof(disable_redirects)
            );
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

void KSMWindowsHostServices::CancelRequest(const std::string& request_id) {
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
