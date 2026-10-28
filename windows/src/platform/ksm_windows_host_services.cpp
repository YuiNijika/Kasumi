#include "ksm_windows_host_services.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "../platform/ksm_virtual_path.h"

namespace {

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

}

KSMWindowsHostServices::KSMWindowsHostServices(HWND window)
    : window_(window) {
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
