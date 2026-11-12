#include "ksm_webview2_host.h"

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <deque>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../bridge/ksm_router.h"
#include "../commands/ksm_filesystem.h"
#include "ksm_host_services.h"
#include "ksm_web_resource.h"
#include "ksm_webview_host.h"
#include "ksm_webview_security.h"
#include "ksm_window_frame.h"
#include "ksm_window_registry.h"
#include "ksm_windows_host_services.h"

namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

constexpr UINT kRunUiTaskMessage = WM_APP + 0x4B;
constexpr wchar_t kViewWindowClass[] = L"KasumiWebView2View";
constexpr wchar_t kSecondaryWindowClass[] = L"KasumiSecondaryWindow";
constexpr wchar_t kLoadingWindowClass[] = L"KasumiWebView2Loading";
constexpr wchar_t kLocalFileFilter[] = L"https://app.local/__ksm_file*";
constexpr char kLocalFileUrlPrefix[] = "https://app.local/__ksm_file";
constexpr UINT_PTR kLoadingTimerId = 0x4B4D;
constexpr UINT kLoadingTimerIntervalMs = 40;
constexpr int kLoadingCardMaxWidthDip = 380;
constexpr int kLoadingCardMaxHeightDip = 190;
constexpr int kLoadingCardMarginDip = 24;
constexpr int kLoadingCompactWidthDip = 240;
constexpr int kLoadingCompactHeightDip = 132;
constexpr int kFailureCardMaxWidthDip = 620;
constexpr int kFailureCardMaxHeightDip = 360;
constexpr int kLoadingDpiBase = 96;
constexpr double kTau = 6.28318530717958647692;

std::atomic<std::uint64_t> g_next_window_id{1};

COREWEBVIEW2_COLOR ParseBackgroundColor(const std::string &value) {
  COREWEBVIEW2_COLOR color{255, 255, 255, 255};
  if (value.empty()) {
    return color;
  }
  std::string hex = value;
  if (!hex.empty() && hex.front() == '#') {
    hex.erase(hex.begin());
  }
  if (hex.size() != 6 && hex.size() != 8) {
    return color;
  }
  const auto nibble = [](char character) -> int {
    if (character >= '0' && character <= '9') {
      return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
      return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
      return character - 'A' + 10;
    }
    return -1;
  };
  const auto byte = [&](std::size_t offset) -> int {
    const int high = nibble(hex[offset]);
    const int low = nibble(hex[offset + 1]);
    if (high < 0 || low < 0) {
      return -1;
    }
    return high * 16 + low;
  };
  if (hex.size() == 6) {
    const int red = byte(0);
    const int green = byte(2);
    const int blue = byte(4);
    if (red < 0 || green < 0 || blue < 0) {
      return color;
    }
    color = COREWEBVIEW2_COLOR{
        static_cast<BYTE>(255),
        static_cast<BYTE>(red),
        static_cast<BYTE>(green),
        static_cast<BYTE>(blue)
    };
    return color;
  }
  const int alpha = byte(0);
  const int red = byte(2);
  const int green = byte(4);
  const int blue = byte(6);
  if (alpha < 0 || red < 0 || green < 0 || blue < 0) {
    return color;
  }
  color = COREWEBVIEW2_COLOR{
      static_cast<BYTE>(alpha),
      static_cast<BYTE>(red),
      static_cast<BYTE>(green),
      static_cast<BYTE>(blue)
  };
  return color;
}

std::optional<COLORREF> ParseOptionalColor(const std::string &value) {
  if (value.empty()) {
    return std::nullopt;
  }
  std::string hex = value;
  if (hex.front() == '#') {
    hex.erase(hex.begin());
  }
  if (hex.size() != 6 && hex.size() != 8) {
    return std::nullopt;
  }
  const auto nibble = [](char character) -> int {
    if (character >= '0' && character <= '9') {
      return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
      return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
      return character - 'A' + 10;
    }
    return -1;
  };
  const auto byte = [&](std::size_t offset) -> int {
    const int high = nibble(hex[offset]);
    const int low = nibble(hex[offset + 1]);
    if (high < 0 || low < 0) {
      return -1;
    }
    return high * 16 + low;
  };
  const std::size_t offset = hex.size() == 8 ? 2 : 0;
  const int red = byte(offset);
  const int green = byte(offset + 2);
  const int blue = byte(offset + 4);
  if (red < 0 || green < 0 || blue < 0) {
    return std::nullopt;
  }
  return RGB(
      static_cast<BYTE>(red),
      static_cast<BYTE>(green),
      static_cast<BYTE>(blue)
  );
}

std::wstring ToWide(const std::string &value) {
  if (value.empty()) {
    return {};
  }
  const int size =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), size);
  return result;
}

std::string ToUtf8(const std::wstring &value) {
  if (value.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                       static_cast<int>(value.size()), nullptr,
                                       0, nullptr, nullptr);
  if (size <= 0) {
    return {};
  }
  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

std::wstring WebViewDebugArguments() {
  wchar_t value[16]{};
  const DWORD length = GetEnvironmentVariableW(
      L"KASUMI_WEBVIEW_DEBUG_PORT", value,
      static_cast<DWORD>(std::size(value)));
  if (length == 0 || length >= std::size(value)) {
    return {};
  }
  wchar_t *end = nullptr;
  const long port = std::wcstol(value, &end, 10);
  if (end == value || *end != L'\0' || port <= 0 || port > 65535) {
    return {};
  }
  return L"--remote-debugging-port=" + std::to_wstring(port) +
         L" --remote-allow-origins=*";
}

std::wstring RuntimeVersionWide() {
  LPWSTR version = nullptr;
  if (FAILED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &version)) ||
      version == nullptr) {
    return {};
  }
  std::wstring result(version);
  CoTaskMemFree(version);
  return result;
}

std::filesystem::path DefaultUserDataFolder() {
  PWSTR local_app_data = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                  nullptr, &local_app_data)) ||
      local_app_data == nullptr) {
    return {};
  }
  std::filesystem::path result(local_app_data);
  CoTaskMemFree(local_app_data);

  wchar_t executable_path[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(
      nullptr, executable_path, static_cast<DWORD>(std::size(executable_path)));
  const std::wstring executable_name =
      length > 0 && length < std::size(executable_path)
          ? std::filesystem::path(executable_path).stem().wstring()
          : L"Kasumi";
  return result / L"Kasumi" / L"WebView2" / executable_name;
}

std::wstring HostLogFilePath() {
  PWSTR local_app_data = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                  nullptr, &local_app_data)) ||
      local_app_data == nullptr) {
    return {};
  }
  const std::filesystem::path result(local_app_data);
  CoTaskMemFree(local_app_data);

  wchar_t executable_path[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(
      nullptr, executable_path, static_cast<DWORD>(std::size(executable_path)));
  const std::wstring executable_name =
      length > 0 && length < std::size(executable_path)
          ? std::filesystem::path(executable_path).stem().wstring()
          : L"Kasumi";
  return (result / L"Kasumi" / L"Logs" / (executable_name + L".log"))
      .wstring();
}

std::wstring LogTimestamp() {
  SYSTEMTIME now{};
  GetLocalTime(&now);
  wchar_t buffer[64]{};
  std::swprintf(
      buffer,
      std::size(buffer),
      L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
      now.wYear,
      now.wMonth,
      now.wDay,
      now.wHour,
      now.wMinute,
      now.wSecond,
      now.wMilliseconds
  );
  return buffer;
}

std::wstring AppendHostLog(const std::string &level,
                           const std::string &message) {
  const std::wstring path = HostLogFilePath();
  if (path.empty()) {
    return {};
  }
  std::error_code error;
  std::filesystem::create_directories(
      std::filesystem::path(path).parent_path(), error);

  const std::wstring line =
      L"[" + LogTimestamp() + L"] [" + ToWide(level) + L"] " +
      ToWide(message) + L"\r\n";
  HANDLE file = CreateFileW(
      path.c_str(),
      FILE_APPEND_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
  );
  if (file != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    WriteFile(
        file,
        line.data(),
        static_cast<DWORD>(line.size() * sizeof(wchar_t)),
        &written,
        nullptr
    );
    CloseHandle(file);
  }
  return path;
}

RECT ParentClientBounds(HWND parent) {
  RECT bounds{};
  if (!parent || !GetClientRect(parent, &bounds)) {
    return {};
  }
  return bounds;
}

RECT ClampToParent(HWND parent, RECT bounds) {
  const RECT client = ParentClientBounds(parent);
  const LONG client_width = client.right - client.left;
  const LONG client_height = client.bottom - client.top;
  if (client_width <= 0 || client_height <= 0) {
    return {};
  }

  bounds.left = std::clamp<LONG>(bounds.left, client.left, client.right - 1);
  bounds.top = std::clamp<LONG>(bounds.top, client.top, client.bottom - 1);
  const LONG maximum_width = client.right - bounds.left;
  const LONG maximum_height = client.bottom - bounds.top;
  bounds.right = bounds.left + std::clamp<LONG>(
                                  bounds.right - bounds.left, 1, maximum_width);
  bounds.bottom = bounds.top + std::clamp<LONG>(
                                    bounds.bottom - bounds.top, 1,
                                    maximum_height);
  return bounds;
}

bool HasArea(const RECT &bounds) {
  return bounds.right > bounds.left && bounds.bottom > bounds.top;
}

std::wstring StripQueryAndFragment(std::wstring url) {
  const auto query = url.find(L'?');
  if (query != std::wstring::npos) {
    url.resize(query);
  }
  const auto fragment = url.find(L'#');
  if (fragment != std::wstring::npos) {
    url.resize(fragment);
  }
  return url;
}

std::string MimeTypeForLocalFile(const std::string &path) {
  const std::size_t dot = path.find_last_of('.');
  const std::size_t slash = path.find_last_of('/');
  if (dot == std::string::npos ||
      (slash != std::string::npos && dot < slash)) {
    return "application/octet-stream";
  }
  std::string extension = path.substr(dot + 1);
  for (char &character : extension) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  if (extension == "jpg" || extension == "jpeg" || extension == "jpe") {
    return "image/jpeg";
  }
  if (extension == "png") {
    return "image/png";
  }
  if (extension == "gif") {
    return "image/gif";
  }
  if (extension == "webp") {
    return "image/webp";
  }
  if (extension == "avif") {
    return "image/avif";
  }
  if (extension == "bmp") {
    return "image/bmp";
  }
  if (extension == "ico") {
    return "image/x-icon";
  }
  if (extension == "svg") {
    return "image/svg+xml";
  }
  if (extension == "mp3") {
    return "audio/mpeg";
  }
  if (extension == "flac") {
    return "audio/flac";
  }
  if (extension == "m4a") {
    return "audio/mp4";
  }
  if (extension == "aac") {
    return "audio/aac";
  }
  if (extension == "ogg" || extension == "opus" || extension == "oga") {
    return "audio/ogg";
  }
  if (extension == "wav") {
    return "audio/wav";
  }
  if (extension == "mp4" || extension == "m4v") {
    return "video/mp4";
  }
  if (extension == "webm") {
    return "video/webm";
  }
  if (extension == "mov") {
    return "video/quicktime";
  }
  if (extension == "json") {
    return "application/json";
  }
  if (extension == "html" || extension == "htm") {
    return "text/html";
  }
  if (extension == "css") {
    return "text/css";
  }
  if (extension == "js" || extension == "mjs") {
    return "text/javascript";
  }
  if (extension == "txt") {
    return "text/plain";
  }
  if (extension == "xml") {
    return "application/xml";
  }
  if (extension == "pdf") {
    return "application/pdf";
  }
  if (extension == "woff") {
    return "font/woff";
  }
  if (extension == "woff2") {
    return "font/woff2";
  }
  if (extension == "ttf") {
    return "font/ttf";
  }
  if (extension == "otf") {
    return "font/otf";
  }
  return "application/octet-stream";
}

bool IsLocalFileUrl(const std::string &url) {
  constexpr std::size_t prefix_size = sizeof(kLocalFileUrlPrefix) - 1;
  if (!url.starts_with(kLocalFileUrlPrefix)) {
    return false;
  }
  return url.size() == prefix_size || url[prefix_size] == '?';
}

std::wstring ResolveRouteUrl(const std::wstring &base_url,
                             const std::string &route,
                             const std::string &url) {
  if (!url.empty()) {
    return ToWide(url);
  }
  if (route.empty()) {
    return base_url;
  }
  const std::wstring wide_route = ToWide(route);
  if (wide_route.find(L"://") != std::wstring::npos) {
    return wide_route;
  }
  std::wstring origin = StripQueryAndFragment(base_url);
  const auto slash = origin.find_last_of(L'/');
  if (slash != std::wstring::npos) {
    origin.resize(slash);
  } else {
    origin.clear();
  }
  std::wstring relative = wide_route;
  if (!relative.empty() && relative.front() == L'/') {
    relative.erase(relative.begin());
  }
  const auto segment = relative.find_last_of(L"/\\");
  const auto dot = relative.find_last_of(L'.');
  if (dot == std::wstring::npos ||
      (segment != std::wstring::npos && dot < segment)) {
    relative += L".html";
  }
  return origin + L"/" + relative;
}

std::wstring AppendWindowId(const std::wstring &url, std::uint64_t window_id) {
  const std::wstring query =
      L"windowId=" + std::to_wstring(window_id);
  const auto fragment = url.find(L'#');
  std::wstring base = fragment == std::wstring::npos
                          ? url
                          : url.substr(0, fragment);
  const std::wstring suffix = fragment == std::wstring::npos
                                  ? std::wstring()
                                  : url.substr(fragment);
  const wchar_t separator =
      base.find(L'?') == std::wstring::npos ? L'?' : L'&';
  return base + separator + query + suffix;
}

int ScaleDip(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), kLoadingDpiBase);
}

COLORREF MixColor(COLORREF from, COLORREF to, int percent) {
  const auto mix = [percent](int first, int second) {
    return first + (second - first) * percent / 100;
  };
  return RGB(
      mix(GetRValue(from), GetRValue(to)),
      mix(GetGValue(from), GetGValue(to)),
      mix(GetBValue(from), GetBValue(to))
  );
}

int ColorLuminance(COLORREF color) {
  return (
      GetRValue(color) * 299 +
      GetGValue(color) * 587 +
      GetBValue(color) * 114
  ) / 1000;
}

struct KSMWebViewLoadingPalette {
  COLORREF background_top;
  COLORREF background_bottom;
  COLORREF card;
  COLORREF card_shadow;
  COLORREF border;
  COLORREF text;
  COLORREF secondary_text;
  COLORREF accent;
  COLORREF logo_foreground;
  COLORREF error;
};

KSMWebViewLoadingPalette LoadingPaletteFor(
    COREWEBVIEW2_COLOR background
) {
  const COLORREF base = RGB(background.R, background.G, background.B);
  if (ColorLuminance(base) < 128) {
    return {
        RGB(17, 18, 23),
        RGB(29, 32, 41),
        RGB(27, 30, 38),
        RGB(11, 13, 18),
        RGB(51, 56, 69),
        RGB(245, 247, 251),
        RGB(157, 164, 179),
        RGB(111, 159, 255),
        RGB(255, 255, 255),
        RGB(255, 107, 102),
    };
  }
  return {
      RGB(249, 250, 253),
      RGB(237, 240, 247),
      RGB(255, 255, 255),
      RGB(220, 225, 235),
      RGB(212, 218, 229),
      RGB(25, 28, 35),
      RGB(104, 111, 126),
      RGB(43, 108, 255),
      RGB(255, 255, 255),
      RGB(212, 60, 56),
  };
}

struct KSMWebViewErrorStyle {
  std::optional<COREWEBVIEW2_COLOR> background;
  std::optional<COLORREF> card;
  std::optional<COLORREF> border;
  std::optional<COLORREF> text;
  std::optional<COLORREF> muted_text;
  std::optional<COLORREF> accent;
  std::optional<COLORREF> error;
  std::wstring title = L"WebView2 初始化失败";
  std::wstring hint = L"请检查 WebView2 Runtime、应用配置或网络后重新启动";
};

KSMWebViewErrorStyle BuildErrorStyle(
    const KSMWebView2ErrorPageOptions &options
) {
  KSMWebViewErrorStyle style;
  if (const auto background = ParseOptionalColor(options.background_color)) {
    style.background = COREWEBVIEW2_COLOR{
        255,
        static_cast<BYTE>(GetRValue(*background)),
        static_cast<BYTE>(GetGValue(*background)),
        static_cast<BYTE>(GetBValue(*background)),
    };
  }
  style.card = ParseOptionalColor(options.card_color);
  style.border = ParseOptionalColor(options.border_color);
  style.text = ParseOptionalColor(options.text_color);
  style.muted_text = ParseOptionalColor(options.muted_text_color);
  style.accent = ParseOptionalColor(options.accent_color);
  style.error = ParseOptionalColor(options.error_color);
  if (!options.title.empty()) {
    style.title = ToWide(options.title);
  }
  if (!options.hint.empty()) {
    style.hint = ToWide(options.hint);
  }
  return style;
}

KSMWebViewLoadingPalette ApplyErrorStyle(
    KSMWebViewLoadingPalette palette,
    const KSMWebViewErrorStyle &style
) {
  if (style.background) {
    const COLORREF base = RGB(
        style.background->R,
        style.background->G,
        style.background->B
    );
    palette.background_top = base;
    palette.background_bottom = MixColor(
        base,
        ColorLuminance(base) < 128 ? RGB(0, 0, 0) : RGB(255, 255, 255),
        12
    );
  }
  if (style.card) {
    palette.card = *style.card;
  }
  if (style.border) {
    palette.border = *style.border;
  }
  if (style.text) {
    palette.text = *style.text;
  }
  if (style.muted_text) {
    palette.secondary_text = *style.muted_text;
  }
  if (style.accent) {
    palette.accent = *style.accent;
  }
  if (style.error) {
    palette.error = *style.error;
  }
  return palette;
}

std::string WebErrorStatusLabel(COREWEBVIEW2_WEB_ERROR_STATUS status) {
  switch (status) {
  case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_COMMON_NAME_IS_INCORRECT:
    return "证书域名不匹配";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_EXPIRED:
    return "证书已过期";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CLIENT_CERTIFICATE_CONTAINS_ERRORS:
    return "客户端证书无效";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_REVOKED:
    return "证书已被吊销";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_IS_INVALID:
    return "证书无效";
  case COREWEBVIEW2_WEB_ERROR_STATUS_SERVER_UNREACHABLE:
    return "服务器不可达";
  case COREWEBVIEW2_WEB_ERROR_STATUS_TIMEOUT:
    return "连接超时";
  case COREWEBVIEW2_WEB_ERROR_STATUS_ERROR_HTTP_INVALID_SERVER_RESPONSE:
    return "服务器返回了无效响应";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED:
    return "连接已中止";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_RESET:
    return "连接被重置";
  case COREWEBVIEW2_WEB_ERROR_STATUS_DISCONNECTED:
    return "网络已断开";
  case COREWEBVIEW2_WEB_ERROR_STATUS_CANNOT_CONNECT:
    return "无法连接到服务器";
  case COREWEBVIEW2_WEB_ERROR_STATUS_HOST_NAME_NOT_RESOLVED:
    return "无法解析主机名";
  case COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED:
    return "导航被取消";
  case COREWEBVIEW2_WEB_ERROR_STATUS_REDIRECT_FAILED:
    return "重定向失败";
  case COREWEBVIEW2_WEB_ERROR_STATUS_VALID_AUTHENTICATION_CREDENTIALS_REQUIRED:
    return "需要有效的身份验证凭据";
  case COREWEBVIEW2_WEB_ERROR_STATUS_VALID_PROXY_AUTHENTICATION_REQUIRED:
    return "需要有效的代理身份验证凭据";
  case COREWEBVIEW2_WEB_ERROR_STATUS_UNEXPECTED_ERROR:
    return "发生意外错误";
  case COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN:
  default:
    return "未知 WebView2 错误";
  }
}

std::wstring ParentWindowTitle(HWND parent) {
  wchar_t title[256]{};
  const int length = GetWindowTextW(
      parent, title, static_cast<int>(std::size(title))
  );
  if (length <= 0) {
    return L"Kasumi";
  }
  return std::wstring(title, static_cast<std::size_t>(length));
}

class KSMWebViewLoadingSurface final {
public:
  KSMWebViewLoadingSurface() = default;
  ~KSMWebViewLoadingSurface() { Close(); }

  KSMWebViewLoadingSurface(const KSMWebViewLoadingSurface &) = delete;
  KSMWebViewLoadingSurface &operator=(
      const KSMWebViewLoadingSurface &
  ) = delete;

  bool Create(
      HWND parent,
      RECT bounds,
      COREWEBVIEW2_COLOR background,
      bool visible,
      const KSMWebViewErrorStyle &error_style = {}
  ) {
    Close();
    if (!parent || !EnsureWindowClass()) {
      return false;
    }
    background_ = background;
    title_ = ParentWindowTitle(parent);
    visible_ = visible;
    error_style_ = error_style;
    failure_ = false;
    error_message_.clear();
    log_path_.clear();
    window_ = CreateWindowExW(
        0,
        kLoadingWindowClass,
        L"",
        WS_CHILD | WS_CLIPSIBLINGS |
            (visible ? WS_VISIBLE : 0),
        bounds.left,
        bounds.top,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        parent,
        nullptr,
        GetModuleHandleW(nullptr),
        this
    );
    if (!window_) {
      return false;
    }
    SetWindowLongPtrW(
        window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)
    );
    SetTimer(window_, kLoadingTimerId, kLoadingTimerIntervalMs, nullptr);
    return true;
  }

  void SetBounds(RECT bounds) {
    if (!window_ || !IsWindow(window_)) {
      return;
    }
    SetWindowPos(
        window_,
        HWND_TOP,
        bounds.left,
        bounds.top,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        SWP_NOACTIVATE |
            (visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)
    );
    InvalidateRect(window_, nullptr, FALSE);
  }

  void SetVisible(bool visible) {
    visible_ = visible;
    if (!window_ || !IsWindow(window_)) {
      return;
    }
    ShowWindow(window_, visible ? SW_SHOWNA : SW_HIDE);
    if (visible) {
      Raise();
    }
  }

  void ShowFailure(
      const std::wstring &error_message,
      const std::wstring &log_path
  ) {
    failure_ = true;
    error_message_ = error_message.empty() ? L"未知初始化错误" : error_message;
    log_path_ = log_path;
    visible_ = true;
    if (!window_ || !IsWindow(window_)) {
      return;
    }
    KillTimer(window_, kLoadingTimerId);
    ShowWindow(window_, SW_SHOWNA);
    Raise();
    InvalidateRect(window_, nullptr, FALSE);
  }

  void Raise() {
    if (!window_ || !IsWindow(window_) || !visible_) {
      return;
    }
    SetWindowPos(
        window_,
        HWND_TOP,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
    );
  }

  void Close() {
    DeleteFonts();
    if (!window_) {
      return;
    }
    HWND window = window_;
    window_ = nullptr;
    if (IsWindow(window)) {
      KillTimer(window, kLoadingTimerId);
      SetWindowLongPtrW(window, GWLP_USERDATA, 0);
      DestroyWindow(window);
    }
  }

private:
  static bool EnsureWindowClass() {
    static std::once_flag once;
    static bool registered = false;
    std::call_once(once, [] {
      WNDCLASSEXW window_class{};
      window_class.cbSize = sizeof(window_class);
      window_class.style = CS_HREDRAW | CS_VREDRAW;
      window_class.lpfnWndProc = &KSMWebViewLoadingSurface::WindowProc;
      window_class.hInstance = GetModuleHandleW(nullptr);
      window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      window_class.lpszClassName = kLoadingWindowClass;
      registered = RegisterClassExW(&window_class) != 0 ||
                   GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return registered;
  }

  static LRESULT CALLBACK WindowProc(
      HWND window,
      UINT message,
      WPARAM wparam,
      LPARAM lparam
  ) {
    auto *self = reinterpret_cast<KSMWebViewLoadingSurface *>(
        GetWindowLongPtrW(window, GWLP_USERDATA)
    );
    switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);
      if (self) {
        RECT client{};
        GetClientRect(window, &client);
        self->Paint(dc, client);
      }
      EndPaint(window, &paint);
      return 0;
    }
    case WM_TIMER:
      if (wparam == kLoadingTimerId) {
        InvalidateRect(window, nullptr, FALSE);
        return 0;
      }
      break;
    case WM_SIZE:
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_NCHITTEST:
      return HTCLIENT;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DESTROY:
      KillTimer(window, kLoadingTimerId);
      break;
    case WM_NCDESTROY:
      KillTimer(window, kLoadingTimerId);
      SetWindowLongPtrW(window, GWLP_USERDATA, 0);
      if (self) {
        self->window_ = nullptr;
      }
      return 0;
    default:
      break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  static HFONT CreateUiFont(int size_dip, int weight, UINT dpi) {
    return CreateFontW(
        -ScaleDip(size_dip, dpi),
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei UI"
    );
  }

  void EnsureFonts(UINT dpi) {
    if (font_dpi_ == dpi) {
      return;
    }
    DeleteFonts();
    font_dpi_ = dpi;
    title_font_ = CreateUiFont(17, FW_SEMIBOLD, dpi);
    status_font_ = CreateUiFont(12, FW_NORMAL, dpi);
    subtitle_font_ = CreateUiFont(10, FW_NORMAL, dpi);
  }

  void DeleteFonts() {
    if (title_font_) {
      DeleteObject(title_font_);
      title_font_ = nullptr;
    }
    if (status_font_) {
      DeleteObject(status_font_);
      status_font_ = nullptr;
    }
    if (subtitle_font_) {
      DeleteObject(subtitle_font_);
      subtitle_font_ = nullptr;
    }
    font_dpi_ = 0;
  }

  void PaintBackground(
      HDC dc,
      const RECT &client,
      const KSMWebViewLoadingPalette &palette
  ) {
    TRIVERTEX vertices[2] = {
        {
            client.left,
            client.top,
            static_cast<COLOR16>(GetRValue(palette.background_top) * 257),
            static_cast<COLOR16>(GetGValue(palette.background_top) * 257),
            static_cast<COLOR16>(GetBValue(palette.background_top) * 257),
            0xFF00,
        },
        {
            client.right,
            client.bottom,
            static_cast<COLOR16>(GetRValue(palette.background_bottom) * 257),
            static_cast<COLOR16>(GetGValue(palette.background_bottom) * 257),
            static_cast<COLOR16>(GetBValue(palette.background_bottom) * 257),
            0xFF00,
        },
    };
    GRADIENT_RECT gradient{0, 1};
    if (!GradientFill(
            dc,
            vertices,
            static_cast<ULONG>(std::size(vertices)),
            &gradient,
            1,
            GRADIENT_FILL_RECT_V
        )) {
      HBRUSH brush = CreateSolidBrush(palette.background_bottom);
      FillRect(dc, &client, brush);
      DeleteObject(brush);
    }
  }

  void PaintLogo(
      HDC dc,
      int center_x,
      int top,
      int size,
      const KSMWebViewLoadingPalette &palette,
      UINT dpi
  ) {
    HBRUSH accent = CreateSolidBrush(palette.accent);
    HBRUSH logo_dot = CreateSolidBrush(palette.logo_foreground);
    HPEN border = CreatePen(PS_SOLID, 1, palette.accent);
    HGDIOBJ old_brush = SelectObject(dc, accent);
    HGDIOBJ old_pen = SelectObject(dc, border);
    const int radius = ScaleDip(10, dpi);
    RoundRect(
        dc,
        center_x - size / 2,
        top,
        center_x + size / 2,
        top + size,
        radius,
        radius
    );
    SelectObject(dc, logo_dot);
    SelectObject(dc, GetStockObject(NULL_PEN));
    const int dot_radius = (std::max)(2, ScaleDip(4, dpi));
    Ellipse(
        dc,
        center_x - dot_radius,
        top + size / 2 - dot_radius,
        center_x + dot_radius,
        top + size / 2 + dot_radius
    );
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(border);
    DeleteObject(logo_dot);
    DeleteObject(accent);
  }

  void PaintSpinner(
      HDC dc,
      int center_x,
      int center_y,
      int radius,
      const KSMWebViewLoadingPalette &palette,
      UINT dpi
  ) {
    constexpr int dot_count = 8;
    const double phase =
        static_cast<double>(GetTickCount64() % 1600) / 1600.0;
    const int head = static_cast<int>(phase * dot_count) % dot_count;
    const int dot_radius = (std::max)(1, ScaleDip(2, dpi));
    for (int index = 0; index < dot_count; ++index) {
      const double angle =
          kTau * static_cast<double>(index) / dot_count;
      const int x = center_x + static_cast<int>(
          std::lround(std::cos(angle) * radius)
      );
      const int y = center_y + static_cast<int>(
          std::lround(std::sin(angle) * radius)
      );
      const int distance = (head - index + dot_count) % dot_count;
      const COLORREF color = distance < 3
          ? palette.accent
          : MixColor(
              palette.background_bottom, palette.secondary_text, 42
          );
      HBRUSH brush = CreateSolidBrush(color);
      HGDIOBJ old_brush = SelectObject(dc, brush);
      HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
      Ellipse(
          dc,
          x - dot_radius,
          y - dot_radius,
          x + dot_radius,
          y + dot_radius
      );
      SelectObject(dc, old_pen);
      SelectObject(dc, old_brush);
      DeleteObject(brush);
    }
  }

  void DrawCenteredText(
      HDC dc,
      HFONT font,
      COLORREF color,
      const std::wstring &text,
      RECT bounds
  ) {
    if (!font || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
      return;
    }
    HGDIOBJ old_font = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(
        dc,
        text.c_str(),
        static_cast<int>(text.size()),
        &bounds,
        DT_CENTER | DT_SINGLELINE | DT_VCENTER |
            DT_END_ELLIPSIS | DT_NOPREFIX
    );
    SelectObject(dc, old_font);
  }

  void DrawTextBlock(
      HDC dc,
      HFONT font,
      COLORREF color,
      const std::wstring &text,
      RECT bounds,
      UINT flags
  ) {
    if (!font || text.empty() || bounds.right <= bounds.left ||
        bounds.bottom <= bounds.top) {
      return;
    }
    HGDIOBJ old_font = SelectObject(dc, font);
    const int old_mode = SetBkMode(dc, TRANSPARENT);
    const COLORREF old_color = SetTextColor(dc, color);
    DrawTextW(
        dc,
        text.c_str(),
        static_cast<int>(text.size()),
        &bounds,
        flags | DT_NOPREFIX
    );
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_mode);
    SelectObject(dc, old_font);
  }

  void PaintFailure(
      HDC dc,
      const RECT &client,
      const KSMWebViewLoadingPalette &palette,
      UINT dpi
  ) {
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = ScaleDip(20, dpi);
    const int card_width =
        (std::min)(ScaleDip(kFailureCardMaxWidthDip, dpi), width - margin * 2);
    const int card_height =
        (std::min)(ScaleDip(kFailureCardMaxHeightDip, dpi), height - margin * 2);
    if (card_width <= 0 || card_height <= 0) {
      return;
    }

    const bool compact =
        card_width < ScaleDip(360, dpi) || card_height < ScaleDip(250, dpi);
    const int left = client.left + (width - card_width) / 2;
    const int top = client.top + (height - card_height) / 2;
    RECT content{client.left + margin, client.top + margin,
                 client.right - margin, client.bottom - margin};
    if (!compact) {
      const int radius = ScaleDip(18, dpi);
      HBRUSH shadow = CreateSolidBrush(palette.card_shadow);
      HGDIOBJ old_brush = SelectObject(dc, shadow);
      HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
      RoundRect(
          dc,
          left + ScaleDip(2, dpi),
          top + ScaleDip(4, dpi),
          left + card_width + ScaleDip(2, dpi),
          top + card_height + ScaleDip(4, dpi),
          radius,
          radius
      );
      HBRUSH card = CreateSolidBrush(palette.card);
      HPEN border = CreatePen(PS_SOLID, 1, palette.border);
      SelectObject(dc, card);
      SelectObject(dc, border);
      RoundRect(
          dc,
          left,
          top,
          left + card_width,
          top + card_height,
          radius,
          radius
      );
      SelectObject(dc, old_pen);
      SelectObject(dc, old_brush);
      DeleteObject(border);
      DeleteObject(card);
      DeleteObject(shadow);
      content = RECT{
          left + ScaleDip(28, dpi),
          top + ScaleDip(24, dpi),
          left + card_width - ScaleDip(28, dpi),
          top + card_height - ScaleDip(20, dpi),
      };
    }

    const int icon_size = ScaleDip(compact ? 32 : 42, dpi);
    const int center_x = content.left + (content.right - content.left) / 2;
    int cursor = content.top;
    HBRUSH error_brush = CreateSolidBrush(palette.error);
    HGDIOBJ old_brush = SelectObject(dc, error_brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(
        dc,
        center_x - icon_size / 2,
        cursor,
        center_x + icon_size / 2,
        cursor + icon_size
    );
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(error_brush);

    RECT icon_text{
        center_x - icon_size / 2,
        cursor,
        center_x + icon_size / 2,
        cursor + icon_size,
    };
    DrawTextBlock(
        dc,
        title_font_,
        RGB(255, 255, 255),
        L"!",
        icon_text,
        DT_CENTER | DT_SINGLELINE | DT_VCENTER
    );
    cursor += icon_size + ScaleDip(10, dpi);

    const int title_height = ScaleDip(compact ? 22 : 26, dpi);
    RECT title_rect{
        content.left,
        cursor,
        content.right,
        cursor + title_height,
    };
    DrawTextBlock(
        dc,
        title_font_,
        palette.text,
        error_style_.title,
        title_rect,
        DT_CENTER | DT_SINGLELINE | DT_VCENTER
    );
    cursor += title_height + ScaleDip(8, dpi);

    const int message_height = ScaleDip(compact ? 44 : 62, dpi);
    RECT message_rect{
        content.left,
        cursor,
        content.right,
        cursor + message_height,
    };
    DrawTextBlock(
        dc,
        status_font_,
        palette.text,
        error_message_,
        message_rect,
        DT_CENTER | DT_WORDBREAK
    );
    cursor += message_height + ScaleDip(10, dpi);

    if (cursor + ScaleDip(34, dpi) < content.bottom && !log_path_.empty()) {
      RECT log_label_rect{
          content.left,
          cursor,
          content.right,
          cursor + ScaleDip(16, dpi),
      };
      DrawTextBlock(
          dc,
          subtitle_font_,
          palette.secondary_text,
          L"日志文件",
          log_label_rect,
          DT_LEFT | DT_SINGLELINE | DT_VCENTER
      );
      cursor += ScaleDip(16, dpi);
      RECT log_path_rect{
          content.left,
          cursor,
          content.right,
          cursor + ScaleDip(18, dpi),
      };
      DrawTextBlock(
          dc,
          subtitle_font_,
          palette.accent,
          log_path_,
          log_path_rect,
          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS
      );
      cursor += ScaleDip(18, dpi);
    }

    if (cursor + ScaleDip(16, dpi) <= content.bottom) {
      RECT footer_rect{
          content.left,
          content.bottom - ScaleDip(16, dpi),
          content.right,
          content.bottom,
      };
      DrawTextBlock(
          dc,
          subtitle_font_,
          MixColor(palette.secondary_text, palette.card, 25),
          error_style_.hint,
          footer_rect,
          DT_CENTER | DT_SINGLELINE | DT_VCENTER
      );
    }
  }

  void Paint(HDC target, const RECT &client) {
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
      return;
    }
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (!dc || !bitmap) {
      if (bitmap) {
        DeleteObject(bitmap);
      }
      if (dc) {
        DeleteDC(dc);
      }
      return;
    }
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
    KSMWebViewLoadingPalette palette =
        LoadingPaletteFor(background_);
    if (failure_) {
      palette = ApplyErrorStyle(palette, error_style_);
    }
    PaintBackground(dc, client, palette);

    const UINT dpi = GetDpiForWindow(window_);
    EnsureFonts(dpi);
    if (failure_) {
      PaintFailure(dc, client, palette, dpi);
      BitBlt(
          target,
          client.left,
          client.top,
          width,
          height,
          dc,
          client.left,
          client.top,
          SRCCOPY
      );
      SelectObject(dc, old_bitmap);
      DeleteObject(bitmap);
      DeleteDC(dc);
      return;
    }
    const int margin = ScaleDip(kLoadingCardMarginDip, dpi);
    const int card_width =
        (std::min)(ScaleDip(kLoadingCardMaxWidthDip, dpi), width - margin * 2);
    const int card_height =
        (std::min)(ScaleDip(kLoadingCardMaxHeightDip, dpi), height - margin * 2);
    const bool compact =
        card_width < ScaleDip(kLoadingCompactWidthDip, dpi) ||
        card_height < ScaleDip(kLoadingCompactHeightDip, dpi);

    RECT content{
        client.left + margin,
        client.top + margin,
        client.right - margin,
        client.bottom - margin,
    };
    if (!compact) {
      const int left = client.left + (width - card_width) / 2;
      const int top = client.top + (height - card_height) / 2;
      const int radius = ScaleDip(18, dpi);
      HBRUSH shadow = CreateSolidBrush(palette.card_shadow);
      HGDIOBJ old_brush = SelectObject(dc, shadow);
      HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
      RoundRect(
          dc,
          left + ScaleDip(2, dpi),
          top + ScaleDip(4, dpi),
          left + card_width + ScaleDip(2, dpi),
          top + card_height + ScaleDip(4, dpi),
          radius,
          radius
      );
      HBRUSH card = CreateSolidBrush(palette.card);
      HPEN border = CreatePen(PS_SOLID, 1, palette.border);
      SelectObject(dc, card);
      SelectObject(dc, border);
      RoundRect(
          dc,
          left,
          top,
          left + card_width,
          top + card_height,
          radius,
          radius
      );
      SelectObject(dc, old_pen);
      SelectObject(dc, old_brush);
      DeleteObject(border);
      DeleteObject(card);
      DeleteObject(shadow);
      content = RECT{
          left + ScaleDip(24, dpi),
          top + ScaleDip(20, dpi),
          left + card_width - ScaleDip(24, dpi),
          top + card_height - ScaleDip(20, dpi),
      };
    }

    const int content_height = content.bottom - content.top;
    const int logo_size = ScaleDip(compact ? 24 : 34, dpi);
    const int title_height = ScaleDip(compact ? 20 : 24, dpi);
    const int status_height = ScaleDip(compact ? 18 : 22, dpi);
    const int subtitle_height = ScaleDip(16, dpi);
    const int gap_after_logo = ScaleDip(compact ? 7 : 11, dpi);
    const int gap_after_title = ScaleDip(compact ? 5 : 9, dpi);
    const int stack_height =
        logo_size + gap_after_logo + title_height + gap_after_title +
        status_height +
        (compact ? 0 : gap_after_title + subtitle_height);
    const int stack_top = content.top + (content_height - stack_height) / 2;
    const int center_x = content.left + (content.right - content.left) / 2;

    PaintLogo(
        dc, center_x, stack_top, logo_size, palette, dpi
    );

    int cursor = stack_top + logo_size + gap_after_logo;
    RECT title_rect{
        content.left,
        cursor,
        content.right,
        cursor + title_height,
    };
    DrawCenteredText(dc, title_font_, palette.text, title_, title_rect);
    cursor += title_height + gap_after_title;

    const std::wstring status = L"WebView2 正在初始化";
    SIZE status_size{};
    HGDIOBJ old_status_font = SelectObject(dc, status_font_);
    GetTextExtentPoint32W(
        dc,
        status.c_str(),
        static_cast<int>(status.size()),
        &status_size
    );
    SelectObject(dc, old_status_font);
    const int spinner_radius = ScaleDip(compact ? 8 : 9, dpi);
    const int spinner_gap = ScaleDip(10, dpi);
    const int row_width =
        spinner_radius * 2 + spinner_gap + status_size.cx;
    const int row_left = center_x - row_width / 2;
    PaintSpinner(
        dc,
        row_left + spinner_radius,
        cursor + status_height / 2,
        spinner_radius,
        palette,
        dpi
    );
    RECT status_rect{
        row_left + spinner_radius * 2 + spinner_gap,
        cursor,
        row_left + row_width,
        cursor + status_height,
    };
    DrawCenteredText(
        dc, status_font_, palette.secondary_text, status, status_rect
    );
    cursor += status_height + gap_after_title;

    if (!compact) {
      RECT subtitle_rect{
          content.left,
          cursor,
          content.right,
          cursor + subtitle_height,
      };
      DrawCenteredText(
          dc,
          subtitle_font_,
          MixColor(palette.secondary_text, palette.card, 25),
          L"正在准备渲染引擎，首次启动可能需要几秒",
          subtitle_rect
      );
    }

    BitBlt(
        target,
        client.left,
        client.top,
        width,
        height,
        dc,
        client.left,
        client.top,
        SRCCOPY
    );
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
  }

  HWND window_ = nullptr;
  COREWEBVIEW2_COLOR background_{255, 255, 255, 255};
  KSMWebViewErrorStyle error_style_;
  std::wstring title_;
  std::wstring error_message_;
  std::wstring log_path_;
  UINT font_dpi_ = 0;
  HFONT title_font_ = nullptr;
  HFONT status_font_ = nullptr;
  HFONT subtitle_font_ = nullptr;
  bool visible_ = true;
  bool failure_ = false;
};

} // namespace

std::uint64_t AllocateKSMWindowId() { return g_next_window_id++; }

struct KSMWebView2View::Impl
    : public std::enable_shared_from_this<KSMWebView2View::Impl> {
  Impl(HWND parent, std::shared_ptr<KSMFileSystem> file_system,
       std::vector<std::string> allowed_origins,
       std::vector<std::string> allowed_http_origins,
       ComPtr<ICoreWebView2Environment> environment,
       std::filesystem::path web_root, bool smoke_test,
       std::shared_ptr<KSMWebResourceProvider> web_resources,
       std::wstring start_url, bool dev_mode, std::string app_version,
       std::vector<std::string> allowed_commands,
       std::vector<std::shared_ptr<KSMPlugin>> plugins,
       std::vector<std::string> allowed_permissions, bool decorations,
       KSMWebView2ViewOptions options,
       std::shared_ptr<KSMWindowRegistry> window_registry,
       std::function<void(const std::string &, const std::string &)>
           failure_reporter,
       KSMWebViewErrorStyle error_page_style,
       COREWEBVIEW2_COLOR default_background_color)
      : parent(parent),
        window_id_value(options.window_id != 0 ? options.window_id
                                               : AllocateKSMWindowId()),
        file_system(std::move(file_system)),
        allowed_origins(std::move(allowed_origins)),
        allowed_http_origins(std::move(allowed_http_origins)),
        environment(std::move(environment)), web_root(std::move(web_root)),
        smoke_test(smoke_test), web_resources(std::move(web_resources)),
        start_url(std::move(start_url)), dev_mode(dev_mode),
        decorations(decorations), fill_parent(options.fill_parent),
        visible(true), transparent(options.transparent),
        window_registry(std::move(window_registry)),
        failure_reporter(std::move(failure_reporter)),
        error_page_style(std::move(error_page_style)),
        default_background_color(default_background_color),
        services(std::make_shared<KSMWindowsHostServices>(
            parent, this->allowed_origins, this->allowed_http_origins,
            this->file_system, std::move(app_version), std::string{},
            this->window_registry)),
        webview_host(
            [this](const std::string &message) {
              PostToUi([this, message] {
                if (webview && !closed) {
                  const std::wstring wide_message = ToWide(message);
                  webview->PostWebMessageAsString(wide_message.c_str());
                }
              });
            },
            this->file_system, services,
            [this](std::function<void()> task) { PostToUi(std::move(task)); },
            std::move(allowed_commands), dev_mode, std::move(plugins),
            std::move(allowed_permissions),
            std::to_string(window_id_value)) {
    if (this->window_registry) {
      this->window_registry->Register(
          std::to_string(window_id_value), parent,
          [this](const std::string &event, const std::string &payload) {
            if (closed || !webview) {
              return false;
            }
            webview_host.EmitEvent(event, payload);
            return true;
          });
    }
    services->SetEventDispatcher(
        [this](const std::string& event, const std::string& payload) {
            PostToUi([this, event, payload] {
                webview_host.EmitEvent(
                    event,
                    payload
                );
            });
        }
    );
    desired_bounds = options.bounds;
    if (!options.start_url.empty()) {
      this->start_url = std::move(options.start_url);
    }
    append_smoke_query = options.append_smoke_query;
    const RECT requested_bounds =
        fill_parent ? ParentClientBounds(parent) : desired_bounds;
    desired_bounds = HasArea(requested_bounds)
                         ? ClampToParent(parent, requested_bounds)
                         : requested_bounds;
  }

  ~Impl() { Close(); }

  void CreateLoadingSurface() {
    if (transparent || smoke_test || !parent) {
      return;
    }
    loading_surface = std::make_unique<KSMWebViewLoadingSurface>();
    if (!loading_surface->Create(
            parent,
            desired_bounds,
            default_background_color,
            visible,
            error_page_style
        )) {
      loading_surface.reset();
    }
  }

  void ShowViewFailureSurface(
      const std::wstring &error_message,
      const std::wstring &log_path
  ) {
    if (transparent || smoke_test || !parent) {
      return;
    }
    if (!failure_surface_) {
      failure_surface_ = std::make_unique<KSMWebViewLoadingSurface>();
      if (!failure_surface_->Create(
              parent,
              desired_bounds,
              default_background_color,
              visible,
              error_page_style
          )) {
        failure_surface_.reset();
        return;
      }
    }
    failure_surface_->ShowFailure(error_message, log_path);
  }

  void SyncLoadingSurface() {
    if (loading_surface) {
      loading_surface->SetBounds(desired_bounds);
    }
    if (failure_surface_) {
      failure_surface_->SetBounds(desired_bounds);
    }
  }

  void DismissLoadingSurface() {
    loading_surface.reset();
  }

  bool Initialize(InitializationCallback completion_callback) {
    if (closed || initializing || !environment) {
      return false;
    }
    initializing = true;
    completion = std::move(completion_callback);
    if (!EnsureWindowClass()) {
      Fail("The WebView2 child window class could not be registered.");
      return false;
    }

    if (fill_parent) {
      desired_bounds = ParentClientBounds(parent);
    }
    if (!HasArea(desired_bounds)) {
      Fail("The WebView2 view must have non-zero bounds.");
      return false;
    }

    const DWORD style =
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
        (visible ? WS_VISIBLE : 0);
    window = CreateWindowExW(
        0, kViewWindowClass, L"", style, desired_bounds.left,
        desired_bounds.top, desired_bounds.right - desired_bounds.left,
        desired_bounds.bottom - desired_bounds.top, parent, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!window) {
      Fail("The WebView2 child window could not be created.");
      return false;
    }
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    CreateLoadingSurface();

    const std::weak_ptr<Impl> weak = weak_from_this();
    controller_handler =
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [weak](HRESULT result,
                   ICoreWebView2Controller *controller) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed) {
                return S_OK;
              }
              if (FAILED(result) || controller == nullptr) {
                self->Fail("The WebView2 controller could not be created.");
                return S_OK;
              }
              self->controller = controller;
              self->controller->put_IsVisible(self->visible ? TRUE : FALSE);
              ComPtr<ICoreWebView2Controller2> controller2;
              if (
                  SUCCEEDED(controller->QueryInterface(
                      IID_PPV_ARGS(&controller2))) &&
                  controller2
              ) {
                const COREWEBVIEW2_COLOR color = self->transparent
                    ? COREWEBVIEW2_COLOR{0, 0, 0, 0}
                    : self->default_background_color;
                controller2->put_DefaultBackgroundColor(color);
              }
              self->controller->get_CoreWebView2(&self->webview);
              if (!self->webview) {
                self->Fail("The WebView2 instance could not be created.");
                return S_OK;
              }
              self->ConfigureWebView();
              self->NavigateToStartPage();
              self->Complete(true, {});
              return S_OK;
            });

    const HRESULT result = environment->CreateCoreWebView2Controller(
        window, controller_handler.Get());
    if (FAILED(result)) {
      Fail("The WebView2 controller could not be created.");
      return false;
    }
    return true;
  }

  void ConfigureWebView() {
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview->get_Settings(&settings)) && settings) {
      settings->put_IsScriptEnabled(TRUE);
      settings->put_IsWebMessageEnabled(TRUE);
      settings->put_AreDefaultScriptDialogsEnabled(TRUE);
      settings->put_AreDevToolsEnabled(dev_mode ? TRUE : FALSE);
      settings->put_AreDefaultContextMenusEnabled(FALSE);
      settings->put_IsStatusBarEnabled(FALSE);
      if (!decorations) {
        ComPtr<ICoreWebView2Settings9> settings9;
        if (SUCCEEDED(settings.As(&settings9)) && settings9) {
          settings9->put_IsNonClientRegionSupportEnabled(TRUE);
        }
      }
    }

    ConfigureLocalFileResources();

    if (!web_root.empty()) {
      ComPtr<ICoreWebView2_3> webview3;
      if (SUCCEEDED(webview.As(&webview3)) && webview3) {
        const std::wstring folder = web_root.wstring();
        webview3->SetVirtualHostNameToFolderMapping(
            L"app.local", folder.c_str(),
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
      }
    } else if (web_resources) {
      ConfigureEmbeddedResources();
    }

    const std::weak_ptr<Impl> weak = weak_from_this();
    webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed) {
                return S_OK;
              }
              LPWSTR uri = nullptr;
              if (FAILED(args->get_Uri(&uri)) || uri == nullptr) {
                return S_OK;
              }
              const std::string url = ToUtf8(uri);
              CoTaskMemFree(uri);
              if (!KSMWebViewSecurity::IsAllowedNavigation(url, self->dev_mode)) {
                args->put_Cancel(TRUE);
                return S_OK;
              }
              ComPtr<ICoreWebView2NavigationStartingEventArgs3> args3;
              self->pending_navigation_kind =
                  COREWEBVIEW2_NAVIGATION_KIND_NEW_DOCUMENT;
              if (
                  SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&args3))) &&
                  args3
              ) {
                args3->get_NavigationKind(&self->pending_navigation_kind);
              }
              return S_OK;
            })
            .Get(),
        &navigation_starting_token);
    webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed) {
                return S_OK;
              }
              args->put_Handled(TRUE);
              return S_OK;
            })
            .Get(),
        &new_window_token);
    webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed) {
                return S_OK;
              }
              LPWSTR source = nullptr;
              LPWSTR message = nullptr;
              if (FAILED(args->get_Source(&source)) ||
                  FAILED(args->TryGetWebMessageAsString(&message)) ||
                  source == nullptr || message == nullptr) {
                if (source)
                  CoTaskMemFree(source);
                if (message)
                  CoTaskMemFree(message);
                return S_OK;
              }
              self->webview_host.OnMessageReceivedFromOrigin(
                  ToUtf8(message), ToUtf8(source));
              CoTaskMemFree(source);
              CoTaskMemFree(message);
              return S_OK;
            })
            .Get(),
        &message_token);
    webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed) {
                return S_OK;
              }
              BOOL success = FALSE;
              const HRESULT success_result =
                  args->get_IsSuccess(&success);
              if (FAILED(success_result) || !success) {
                COREWEBVIEW2_WEB_ERROR_STATUS status =
                    COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                args->get_WebErrorStatus(&status);
                if (status ==
                    COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED) {
                  self->DismissLoadingSurface();
                  return S_OK;
                }
                const std::string message =
                    "页面加载失败: " + WebErrorStatusLabel(status) +
                    " (WebErrorStatus=" +
                    std::to_string(static_cast<int>(status)) + ")\n" +
                    ToUtf8(self->start_url);
                const std::wstring log_path = AppendHostLog(
                    "ERROR",
                    message + " | runtime=" +
                        KSMWebView2Host::RuntimeVersion()
                );
                if (self->failure_reporter) {
                  self->failure_reporter(message, ToUtf8(log_path));
                }
                self->ShowViewFailureSurface(
                    ToWide(message), log_path
                );
                self->SetVisible(false);
                self->DismissLoadingSurface();
                return S_OK;
              }
              self->DismissLoadingSurface();
              const bool is_reload =
                  self->pending_navigation_kind ==
                  COREWEBVIEW2_NAVIGATION_KIND_RELOAD;
              self->pending_navigation_kind =
                  COREWEBVIEW2_NAVIGATION_KIND_NEW_DOCUMENT;
              self->webview_host.NotifyWebViewNavigated(is_reload);
              return S_OK;
            })
            .Get(),
        &navigation_completed_token);
    webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2SourceChangedEventArgs *args) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed || !args) {
                return S_OK;
              }
              BOOL is_new_document = FALSE;
              if (
                  SUCCEEDED(args->get_IsNewDocument(&is_new_document)) &&
                  is_new_document
              ) {
                self->webview_host.ResetForNavigation();
              }
              return S_OK;
            })
            .Get(),
        &source_changed_token);
    ResizeToClient();
  }

  void ConfigureLocalFileResources() {
    const std::weak_ptr<Impl> weak = weak_from_this();
    webview->AddWebResourceRequestedFilter(
        kLocalFileFilter, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2WebResourceRequestedEventArgs *args)
                -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed || !args || !self->environment) {
                return S_OK;
              }
              const auto respond_error =
                  [self, args](int status, const wchar_t *reason,
                               const char *message) {
                    ComPtr<IStream> content = SHCreateMemStream(
                        reinterpret_cast<const BYTE *>(message),
                        static_cast<UINT>(std::strlen(message)));
                    if (!content) {
                      return;
                    }
                    ComPtr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(
                            self->environment->CreateWebResourceResponse(
                                content.Get(), status, reason,
                                L"Content-Type: text/plain; charset=utf-8\r\n"
                                L"Cache-Control: no-store\r\n",
                                &response))) {
                      args->put_Response(response.Get());
                    }
                  };

              ComPtr<ICoreWebView2WebResourceRequest> request;
              if (FAILED(args->get_Request(&request)) || !request) {
                respond_error(400, L"Bad Request",
                              "The request is invalid.");
                return S_OK;
              }
              LPWSTR uri = nullptr;
              if (FAILED(request->get_Uri(&uri)) || uri == nullptr) {
                respond_error(400, L"Bad Request",
                              "The request URL is missing.");
                return S_OK;
              }
              const std::string url = ToUtf8(uri);
              CoTaskMemFree(uri);

              const auto parsed =
                  KSMWebResourcePath::FromLocalFileUrl(url);
              if (parsed.status == KSMWebLocalFileStatus::Forbidden) {
                respond_error(
                    403, L"Forbidden",
                    "The path is outside the readable sandbox.");
                return S_OK;
              }
              if (parsed.status != KSMWebLocalFileStatus::Ok) {
                respond_error(400, L"Bad Request",
                              "The file path is missing.");
                return S_OK;
              }
              if (!self->file_system) {
                respond_error(404, L"Not Found",
                              "The file was not found.");
                return S_OK;
              }
              const auto file =
                  self->file_system->ResolveReadableFile(parsed.path);
              if (!file) {
                respond_error(404, L"Not Found",
                              "The file was not found.");
                return S_OK;
              }

              std::error_code size_error;
              const auto size =
                  std::filesystem::file_size(*file, size_error);
              ComPtr<IStream> content;
              if (size_error ||
                  FAILED(SHCreateStreamOnFileEx(
                      file->c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                      FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &content)) ||
                  !content) {
                respond_error(404, L"Not Found",
                              "The file could not be opened.");
                return S_OK;
              }

              std::wstring headers =
                  L"Content-Type: " +
                  ToWide(MimeTypeForLocalFile(parsed.path)) + L"\r\n";
              headers += L"Content-Length: " + std::to_wstring(size) +
                         L"\r\n";
              headers += L"Cache-Control: no-cache\r\n";
              ComPtr<ICoreWebView2WebResourceResponse> response;
              if (SUCCEEDED(
                      self->environment->CreateWebResourceResponse(
                          content.Get(), 200, L"OK", headers.c_str(),
                          &response))) {
                args->put_Response(response.Get());
              }
              return S_OK;
            })
            .Get(),
        &local_file_token);
  }

  void ConfigureEmbeddedResources() {
    const std::weak_ptr<Impl> weak = weak_from_this();
    webview->AddWebResourceRequestedFilter(
        L"https://app.local/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [weak](ICoreWebView2 *,
                   ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed || !args || !self->environment ||
                  !self->web_resources) {
                return S_OK;
              }
              ComPtr<ICoreWebView2WebResourceRequest> request;
              if (FAILED(args->get_Request(&request)) || !request) {
                return S_OK;
              }
              LPWSTR uri = nullptr;
              if (FAILED(request->get_Uri(&uri)) || uri == nullptr) {
                return S_OK;
              }
              const std::string url = ToUtf8(uri);
              CoTaskMemFree(uri);
              if (IsLocalFileUrl(url)) {
                return S_OK;
              }

              const auto path = KSMWebResourcePath::FromAppLocalUrl(url);
              const auto resource =
                  path ? self->web_resources->Read(*path) : std::nullopt;
              if (!resource) {
                ComPtr<ICoreWebView2WebResourceResponse> response;
                if (SUCCEEDED(self->environment->CreateWebResourceResponse(
                        nullptr, 404, L"Not Found",
                        L"Content-Type: text/plain\r\n"
                        L"Cache-Control: no-store\r\n",
                        &response))) {
                  args->put_Response(response.Get());
                }
                return S_OK;
              }

              ComPtr<IStream> content = SHCreateMemStream(
                  resource->data.empty() ? nullptr : resource->data.data(),
                  static_cast<UINT>(resource->data.size()));
              if (!content) {
                return S_OK;
              }
              const std::wstring mime = ToWide(resource->mime_type);
              std::wstring headers = L"Content-Type: " + mime + L"\r\n";
              if (*path == "index.html") {
                headers += L"Content-Security-Policy: ";
                headers += ToWide(
                    KSMWebViewSecurity::ContentSecurityPolicy(
                        self->allowed_http_origins
                    ));
                headers += L"\r\n";
              }
              headers += L"Cache-Control: no-cache\r\n";
              ComPtr<ICoreWebView2WebResourceResponse> response;
              if (SUCCEEDED(self->environment->CreateWebResourceResponse(
                      content.Get(), 200, L"OK", headers.c_str(), &response))) {
                args->put_Response(response.Get());
              }
              return S_OK;
            })
            .Get(),
        &web_resource_token);
  }

  void NavigateToStartPage() {
    if (!webview || start_url.empty()) {
      DismissLoadingSurface();
      return;
    }
    std::wstring url = start_url;
    if (smoke_test && append_smoke_query) {
      url += url.find(L'?') == std::wstring::npos ? L"?smoke=1" : L"&smoke=1";
    }
    if (FAILED(webview->Navigate(url.c_str()))) {
      DismissLoadingSurface();
    }
  }

  void ResizeToClient() {
    if (!window || !controller) {
      return;
    }
    RECT client{};
    if (!GetClientRect(window, &client)) {
      return;
    }
    controller->put_Bounds(client);
    if (loading_surface) {
      loading_surface->Raise();
    }
    KSMWindowFrame::RefreshResizeHitTesting(parent);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width > 0 && height > 0) {
      webview_host.NotifyWindowResized(width, height);
    }
  }

  bool ApplyBounds(RECT bounds, bool update_fill_parent) {
    if (!parent || closed) {
      return false;
    }
    if (!HasArea(bounds)) {
      return false;
    }
    bounds = ClampToParent(parent, bounds);
    if (!HasArea(bounds)) {
      return false;
    }
    desired_bounds = bounds;
    if (update_fill_parent) {
      fill_parent = false;
    }
    if (!window) {
      return true;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (!SetWindowPos(window, nullptr, bounds.left, bounds.top, width, height,
                      SWP_NOZORDER | SWP_NOACTIVATE |
                          (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW))) {
      return false;
    }
    SyncLoadingSurface();
    KSMWindowFrame::RefreshResizeHitTesting(parent);
    return true;
  }

  bool FillParentBounds() {
    if (closed || !parent) {
      return false;
    }
    RECT client = ParentClientBounds(parent);
    if (!HasArea(client) || !ApplyBounds(client, false)) {
      return false;
    }
    fill_parent = true;
    return true;
  }

  void PrepareParentResize(int width, int height) {
    if (closed || !window || width <= 0 || height <= 0) {
      return;
    }
    desired_bounds = RECT{0, 0, width, height};
    SetWindowPos(
        window,
        nullptr,
        0,
        0,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE |
            (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)
    );
    SyncLoadingSurface();
  }

  bool SetVisible(bool next_visible) {
    if (closed) {
      return false;
    }
    visible = next_visible;
    if (window) {
      ShowWindow(window, visible ? SW_SHOWNA : SW_HIDE);
    }
    if (loading_surface) {
      loading_surface->SetVisible(visible);
    }
    if (failure_surface_) {
      failure_surface_->SetVisible(true);
      failure_surface_->Raise();
    }
    if (controller) {
      controller->put_IsVisible(visible ? TRUE : FALSE);
    }
    return true;
  }

  void FocusView() {
    if (closed || !window) {
      return;
    }
    if (!visible) {
      SetVisible(true);
    }
    SetFocus(window);
    MoveWebViewFocus();
  }

  void MoveWebViewFocus() {
    if (controller) {
      controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
  }

  bool NavigateTo(const std::wstring &url) {
    if (closed || !webview || url.empty()) {
      return false;
    }
    return SUCCEEDED(webview->Navigate(url.c_str()));
  }

  bool ReloadPage() {
    return !closed && webview && SUCCEEDED(webview->Reload());
  }

  void RunUiTasks() {
    std::deque<std::function<void()>> tasks;
    {
      std::lock_guard lock(ui_mutex);
      tasks.swap(ui_tasks);
    }
    for (auto &task : tasks) {
      try {
        task();
      } catch (...) {
        // A failed UI callback must not break the host message loop.
      }
    }
  }

  void PostToUi(std::function<void()> task) {
    HWND target = nullptr;
    {
      std::lock_guard lock(ui_mutex);
      if (closed || !window) {
        return;
      }
      ui_tasks.push_back(std::move(task));
      target = window;
    }
    PostMessageW(target, kRunUiTaskMessage, 0, 0);
  }

  void Complete(bool success, const std::string &message) {
    if (completed) {
      return;
    }
    completed = true;
    if (completion) {
      auto callback = std::move(completion);
      completion = {};
      callback(success, message);
    }
  }

  void Fail(const std::string &message) {
    const std::wstring log_path = AppendHostLog(
        "ERROR",
        message + " | runtime=" + KSMWebView2Host::RuntimeVersion() +
            " | url=" + ToUtf8(start_url)
    );
    if (failure_reporter) {
      failure_reporter(message, ToUtf8(log_path));
    }
    ShowViewFailureSurface(ToWide(message), log_path);
    Complete(false, message);
    Close();
  }

  void DetachEvents() {
    if (!webview) {
      return;
    }
    if (message_token.value != 0) {
      webview->remove_WebMessageReceived(message_token);
    }
    if (navigation_starting_token.value != 0) {
      webview->remove_NavigationStarting(navigation_starting_token);
    }
    if (new_window_token.value != 0) {
      webview->remove_NewWindowRequested(new_window_token);
    }
    if (navigation_completed_token.value != 0) {
      webview->remove_NavigationCompleted(navigation_completed_token);
    }
    if (source_changed_token.value != 0) {
      webview->remove_SourceChanged(source_changed_token);
    }
    if (web_resource_token.value != 0) {
      webview->remove_WebResourceRequested(web_resource_token);
    }
    if (local_file_token.value != 0) {
      webview->remove_WebResourceRequested(local_file_token);
    }
    if (!web_root.empty()) {
      ComPtr<ICoreWebView2_3> webview3;
      if (SUCCEEDED(webview.As(&webview3)) && webview3) {
        webview3->ClearVirtualHostNameToFolderMapping(L"app.local");
      }
    }
  }

  void ReleaseWebView() {
    DetachEvents();
    webview.Reset();
    if (controller) {
      controller->Close();
      controller.Reset();
    }
    environment.Reset();
    controller_handler.Reset();
  }

  void OnChildWindowDestroyed(HWND destroyed_window) {
    if (destroyed_window != window) {
      return;
    }
    window = nullptr;
    closed = true;
    {
      std::lock_guard lock(ui_mutex);
      ui_tasks.clear();
    }
    ReleaseWebView();
    DismissLoadingSurface();
    if (!completed) {
      Complete(false, "The WebView2 view was destroyed.");
    }
  }

  void Close() {
    if (closed && !window) {
      return;
    }
    closed = true;
    if (window_registry) {
      window_registry->Unregister(std::to_string(window_id_value));
    }
    {
      std::lock_guard lock(ui_mutex);
      ui_tasks.clear();
    }
    ReleaseWebView();
    DismissLoadingSurface();
    HWND child = window;
    window = nullptr;
    if (child && IsWindow(child)) {
      SetWindowLongPtrW(child, GWLP_USERDATA, 0);
      DestroyWindow(child);
    }
    if (!completed) {
      Complete(false, "The WebView2 view was closed.");
    }
  }

  static bool EnsureWindowClass() {
    static std::once_flag once;
    static bool registered = false;
    std::call_once(once, [] {
      WNDCLASSEXW window_class{};
      window_class.cbSize = sizeof(window_class);
      window_class.style = CS_HREDRAW | CS_VREDRAW;
      window_class.lpfnWndProc = &Impl::WindowProc;
      window_class.hInstance = GetModuleHandleW(nullptr);
      window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      window_class.lpszClassName = kViewWindowClass;
      registered = RegisterClassExW(&window_class) != 0 ||
                   GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return registered;
  }

  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam) {
    auto *self = reinterpret_cast<Impl *>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCDESTROY) {
      SetWindowLongPtrW(window, GWLP_USERDATA, 0);
      if (self) {
        self->OnChildWindowDestroyed(window);
      }
    } else if (self) {
      if (message == kRunUiTaskMessage) {
        self->RunUiTasks();
        return 0;
      }
      if (message == WM_SIZE) {
        self->ResizeToClient();
      } else if (message == WM_SETFOCUS) {
        self->MoveWebViewFocus();
      }
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  static std::atomic<std::uint64_t> next_window_id;

  HWND parent = nullptr;
  HWND window = nullptr;
  std::uint64_t window_id_value = 0;
  std::shared_ptr<KSMFileSystem> file_system;
  std::vector<std::string> allowed_origins;
  std::vector<std::string> allowed_http_origins;
  ComPtr<ICoreWebView2Environment> environment;
  std::filesystem::path web_root;
  bool smoke_test = false;
  std::shared_ptr<KSMWebResourceProvider> web_resources;
  std::wstring start_url;
  bool dev_mode = false;
  bool decorations = true;
  bool append_smoke_query = true;
  bool fill_parent = true;
  bool visible = true;
  bool transparent = false;
  std::shared_ptr<KSMWindowRegistry> window_registry;
  std::function<void(const std::string &, const std::string &)>
      failure_reporter;
  KSMWebViewErrorStyle error_page_style;
  COREWEBVIEW2_COLOR default_background_color{255, 255, 255, 255};
  std::unique_ptr<KSMWebViewLoadingSurface> loading_surface;
  std::unique_ptr<KSMWebViewLoadingSurface> failure_surface_;
  RECT desired_bounds{};
  std::shared_ptr<KSMWindowsHostServices> services;
  KSMWebViewHost webview_host;
  ComPtr<ICoreWebView2Controller> controller;
  ComPtr<ICoreWebView2> webview;
  ComPtr<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>
      controller_handler;
  EventRegistrationToken navigation_starting_token{};
  EventRegistrationToken new_window_token{};
  EventRegistrationToken message_token{};
  EventRegistrationToken navigation_completed_token{};
  EventRegistrationToken source_changed_token{};
  EventRegistrationToken local_file_token{};
  EventRegistrationToken web_resource_token{};
  COREWEBVIEW2_NAVIGATION_KIND pending_navigation_kind{
      COREWEBVIEW2_NAVIGATION_KIND_NEW_DOCUMENT};
  std::mutex ui_mutex;
  std::deque<std::function<void()>> ui_tasks;
  InitializationCallback completion;
  bool initializing = false;
  bool completed = false;
  bool closed = false;
};

std::atomic<std::uint64_t> KSMWebView2View::Impl::next_window_id{1};

struct KSMWebView2Host::Impl
    : public std::enable_shared_from_this<KSMWebView2Host::Impl> {
  Impl(HWND window, std::shared_ptr<KSMFileSystem> file_system,
       std::vector<std::string> allowed_origins,
       std::vector<std::string> allowed_http_origins,
       std::filesystem::path web_root, std::filesystem::path user_data_folder,
       bool smoke_test, std::shared_ptr<KSMWebResourceProvider> web_resources,
       std::wstring start_url, bool dev_mode, std::string app_version,
       std::vector<std::string> allowed_commands,
       std::vector<std::shared_ptr<KSMPlugin>> plugins,
       std::vector<std::string> allowed_permissions, bool decorations,
       std::string background_color,
       KSMWebView2ErrorPageOptions error_page)
 : window(window), file_system(std::move(file_system)),
 allowed_origins(std::move(allowed_origins)),
 allowed_http_origins(std::move(allowed_http_origins)),
 web_root(std::move(web_root)),
 user_data_folder(std::move(user_data_folder)), smoke_test(smoke_test),
 web_resources(std::move(web_resources)),
 start_url(std::move(start_url)), dev_mode(dev_mode),
 app_version(std::move(app_version)),
 allowed_commands(std::move(allowed_commands)),
 allowed_permissions(std::move(allowed_permissions)),
 plugins(std::move(plugins)),
 decorations(decorations),
 background_color(ParseBackgroundColor(background_color)),
 error_page_style_(BuildErrorStyle(error_page)) {
    window_registry_ = std::make_shared<KSMWindowRegistry>();
    SetWindowSubclass(window, &Impl::SubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(this));
  }

  ~Impl() {
    Close();
    if (window && IsWindow(window)) {
      RemoveWindowSubclass(window, &Impl::SubclassProc,
                           reinterpret_cast<UINT_PTR>(this));
      window = nullptr;
    }
    failure_surface_.reset();
  }

  void DispatchSystemMessage(
      unsigned int message,
      std::uintptr_t wparam
  ) {
    for (const auto& entry : views) {
      entry.second->DispatchSystemMessage(message, wparam);
    }
  }

  bool Start(bool create_default_view,
             InitializationCallback completion_callback) {
    if (closed || started || initializing) {
      return false;
    }
    ui_thread_id_ = GetCurrentThreadId();
    const std::weak_ptr<Impl> self = weak_from_this();
    window_registry_->SetCreator(
        [self](const std::string &route, const KSMWindowState &state)
            -> std::optional<std::string> {
          const auto host = self.lock();
          if (!host) {
            return std::nullopt;
          }
          return host->CreateWindowInstance(route, state);
        });
    initializing = true;
    started = true;
    should_create_default_view = create_default_view;
    completion = std::move(completion_callback);
    if (!KSMWebView2Host::IsRuntimeAvailable()) {
      Fail("The Evergreen WebView2 Runtime is not installed.");
      return false;
    }
    if (create_default_view && !smoke_test) {
      startup_surface_ = std::make_unique<KSMWebViewLoadingSurface>();
      if (!startup_surface_->Create(
              window,
              ParentClientBounds(window),
              background_color,
              true,
              error_page_style_
          )) {
        startup_surface_.reset();
      }
    }
    if (!user_data_folder.empty()) {
      std::error_code error;
      std::filesystem::create_directories(user_data_folder, error);
      if (error) {
        Fail("The WebView2 data directory could not be created.");
        return false;
      }
    }

    const std::weak_ptr<Impl> weak = weak_from_this();
    environment_handler =
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [weak](HRESULT result,
                   ICoreWebView2Environment *environment) -> HRESULT {
              const auto self = weak.lock();
              if (!self || self->closed) {
                return S_OK;
              }
              if (FAILED(result) || environment == nullptr) {
                self->Fail("The WebView2 environment could not be created.");
                return S_OK;
              }
              self->environment = environment;
              if (self->should_create_default_view) {
                self->CreateDefaultView();
              } else {
                self->Complete(true, {});
              }
              return S_OK;
            });

    std::wstring additional_browser_arguments = WebViewDebugArguments();
#if defined(KASUMI_ENABLE_SMOKE_TEST) && KASUMI_ENABLE_SMOKE_TEST
    if (smoke_test) {
      if (!additional_browser_arguments.empty()) {
        additional_browser_arguments += L" ";
      }
      additional_browser_arguments += L"--no-sandbox";
    }
#endif
    ComPtr<CoreWebView2EnvironmentOptions> options;
    if (!additional_browser_arguments.empty()) {
      options = Make<CoreWebView2EnvironmentOptions>();
      options->put_AdditionalBrowserArguments(
          additional_browser_arguments.c_str());
    }
    const std::filesystem::path resolved_user_data_folder =
        user_data_folder.empty() ? DefaultUserDataFolder() : user_data_folder;
    const std::wstring data_folder = resolved_user_data_folder.wstring();
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, data_folder.empty() ? nullptr : data_folder.c_str(),
        options.Get(), environment_handler.Get());
    if (FAILED(result)) {
      Fail("WebView2 initialization failed.");
      return false;
    }
    return true;
  }

  std::shared_ptr<KSMWebView2View::Impl> CreateViewState(
      const KSMWebView2ViewOptions &options,
      HWND parent_override = nullptr) {
    if (closed || !environment) {
      return nullptr;
    }
    const std::weak_ptr<Impl> weak = weak_from_this();
    auto failure_reporter = [weak](
                                const std::string &error,
                                const std::string &log_path
                            ) {
      const auto self = weak.lock();
      if (self) {
        self->RecordFailure(error, log_path);
      }
    };
    return std::make_shared<KSMWebView2View::Impl>(
        parent_override ? parent_override : window, file_system,
        allowed_origins, allowed_http_origins, environment,
        web_root, smoke_test, web_resources, start_url, dev_mode, app_version,
        options.allowed_commands.empty() ? allowed_commands
                                         : options.allowed_commands,
        plugins,
        options.allowed_permissions.empty() ? allowed_permissions
                                            : options.allowed_permissions,
        decorations, options, window_registry_, std::move(failure_reporter),
        error_page_style_, background_color);
  }

  std::shared_ptr<KSMWebView2View> CreateView(
      const KSMWebView2ViewOptions &options,
      KSMWebView2View::InitializationCallback callback,
      HWND parent_override = nullptr) {
    if (closed || !environment) {
      if (callback) {
        callback(false, "The WebView2 session is not initialized.");
      }
      return nullptr;
    }
    auto state = CreateViewState(options, parent_override);
    if (!state) {
      if (callback) {
        callback(false, "The WebView2 view could not be created.");
      }
      return nullptr;
    }
    auto view = std::shared_ptr<KSMWebView2View>(
        new KSMWebView2View(std::move(state)));
    views[view->window_id()] = view;
    if (!view->Initialize(std::move(callback))) {
      views.erase(view->window_id());
      failed_views_.push_back(std::move(view));
      return nullptr;
    }
    return view;
  }

  void CreateDefaultView() {
    KSMWebView2ViewOptions options;
    options.start_url = start_url;
    options.fill_parent = true;
    options.append_smoke_query = smoke_test;
    options.allowed_commands = allowed_commands;
    options.allowed_permissions = allowed_permissions;
    const std::weak_ptr<Impl> weak = weak_from_this();
    auto view = CreateView(options, [weak](bool success,
                                           const std::string &message) {
      const auto self = weak.lock();
      if (!self || self->closed) {
        return;
      }
      self->Complete(success, message);
    });
    if (!view) {
      return;
    }
    default_view = std::move(view);
    startup_surface_.reset();
  }

  struct SecondaryWindow {
    HWND window = nullptr;
    std::string id;
    std::shared_ptr<KSMWebView2View> view;
    std::weak_ptr<Impl> host;
  };

  static bool EnsureSecondaryWindowClass() {
    static std::once_flag once;
    static bool registered = false;
    std::call_once(once, [] {
      WNDCLASSEXW window_class{};
      window_class.cbSize = sizeof(window_class);
      window_class.style = CS_HREDRAW | CS_VREDRAW;
      window_class.lpfnWndProc = &Impl::SecondaryWindowProc;
      window_class.hInstance = GetModuleHandleW(nullptr);
      window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      window_class.hbrBackground = nullptr;
      window_class.lpszClassName = kSecondaryWindowClass;
      registered = RegisterClassExW(&window_class) != 0 ||
                   GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return registered;
  }

  static LRESULT CALLBACK SecondaryWindowProc(
      HWND window,
      UINT message,
      WPARAM wparam,
      LPARAM lparam
  ) {
    auto *state = reinterpret_cast<SecondaryWindow *>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_SIZE) {
      if (state && state->view) {
        state->view->FillParent();
      }
      return 0;
    }
    if (message == WM_CLOSE) {
      DestroyWindow(window);
      return 0;
    }
    if (message == WM_NCDESTROY) {
      if (state) {
        const auto host = state->host.lock();
        const std::string id = state->id;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (state->view) {
          state->view->Close();
        }
        if (host) {
          host->OnSecondaryWindowDestroyed(id);
        }
      }
      return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  std::optional<std::string> CreateWindowInstance(
      const std::string &route,
      const KSMWindowState &state
  ) {
    if (GetCurrentThreadId() == ui_thread_id_) {
      return CreateWindowInstanceOnUi(route, state);
    }
    auto result = std::make_shared<
        std::promise<std::optional<std::string>>>();
    auto future = result->get_future();
    PostToHostUi([this, route, state, result] {
      result->set_value(CreateWindowInstanceOnUi(route, state));
    });
    return future.get();
  }

  std::optional<std::string> CreateWindowInstanceOnUi(
      const std::string &route,
      const KSMWindowState &state
  ) {
    if (closed || !environment) {
      return std::nullopt;
    }
    if (!EnsureSecondaryWindowClass()) {
      return std::nullopt;
    }
    const std::uint64_t window_id = AllocateKSMWindowId();
    std::wstring url = ResolveRouteUrl(start_url, route, state.url);
    url = AppendWindowId(url, window_id);

    const int width = state.width > 0 ? state.width : 420;
    const int height = state.height > 0 ? state.height : 260;
    DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    if (state.decorations) {
      style |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
      if (state.resizable) {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
      }
    } else {
      style |= WS_POPUP;
    }
    if (state.visible) {
      style |= WS_VISIBLE;
    }
    DWORD extended_style = state.always_on_top ? WS_EX_TOPMOST : 0;
    if (state.transparent) {
      extended_style |= WS_EX_LAYERED;
    }
    if (state.skip_taskbar || !state.decorations) {
      extended_style |= WS_EX_TOOLWINDOW;
    }

    int x = state.x;
    int y = state.y;
    if (x == 0 && y == 0) {
      RECT work{};
      if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        x = work.left + ((work.right - work.left) - width) / 2;
        y = work.top + ((work.bottom - work.top) - height) / 2;
      }
    }

    const std::wstring title =
        ToWide(state.title.empty() ? std::string("Kasumi") : state.title);
    HWND window = CreateWindowExW(
        extended_style,
        kSecondaryWindowClass,
        title.c_str(),
        style,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (!window) {
      return std::nullopt;
    }
    if (!KSMWindowFrame::Configure(
            window,
            state.decorations,
            state.resizable,
            160,
            80
        )) {
      DestroyWindow(window);
      return std::nullopt;
    }
    if (state.transparent) {
      SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA);
    }

    KSMWebView2ViewOptions options;
    options.start_url = url;
    options.fill_parent = true;
    options.append_smoke_query = false;
    options.transparent = state.transparent;
    options.window_id = window_id;
    options.allowed_commands = allowed_commands;
    options.allowed_permissions = allowed_permissions;

    const std::weak_ptr<Impl> weak = weak_from_this();
    auto view = CreateView(
        options,
        [weak](bool success, const std::string &message) {
          if (success) {
            return;
          }
          const auto host = weak.lock();
          if (host && !host->closed) {
            OutputDebugStringW(ToWide(message).c_str());
          }
        },
        window);
    if (!view) {
      DestroyWindow(window);
      return std::nullopt;
    }

    auto secondary = std::make_unique<SecondaryWindow>();
    secondary->window = window;
    secondary->id = std::to_string(window_id);
    secondary->view = view;
    secondary->host = weak;
    const std::string id = secondary->id;
    secondary_windows_[id] = std::move(secondary);
    SetWindowLongPtrW(
        window,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(secondary_windows_[id].get()));
    view->FillParent();
    ShowWindow(window, state.visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (state.visible) {
      UpdateWindow(window);
    }
    return id;
  }

  void PostToHostUi(std::function<void()> task) {
    {
      std::lock_guard lock(host_ui_mutex_);
      host_ui_tasks_.push_back(std::move(task));
    }
    if (window && IsWindow(window)) {
      PostMessageW(window, kRunUiTaskMessage, 0, 0);
    }
  }

  void RunHostUiTasks() {
    std::deque<std::function<void()>> tasks;
    {
      std::lock_guard lock(host_ui_mutex_);
      tasks.swap(host_ui_tasks_);
    }
    for (auto &task : tasks) {
      task();
    }
  }

  void OnSecondaryWindowDestroyed(const std::string &id) {
    secondary_windows_.erase(id);
  }

  bool DestroyView(std::uint64_t window_id) {
    const auto iterator = views.find(window_id);
    if (iterator == views.end()) {
      return false;
    }
    auto view = std::move(iterator->second);
    views.erase(iterator);
    if (default_view && default_view->window_id() == window_id) {
      default_view.reset();
    }
    view->Close();
    return true;
  }

  void LayoutFillViews() {
    for (const auto &[window_id, view] : views) {
      (void)window_id;
      if (view->fills_parent()) {
        view->FillParent();
      }
    }
  }

  void LayoutFillViewsForSize(int width, int height) {
    for (const auto &[window_id, view] : views) {
      (void)window_id;
      if (view->fills_parent()) {
        view->PrepareParentResize(width, height);
      }
    }
  }

  std::size_t ViewCount() const noexcept { return views.size(); }

  void Complete(bool success, const std::string &message) {
    if (completed) {
      return;
    }
    completed = true;
    if (completion) {
      auto callback = std::move(completion);
      completion = {};
      callback(success, message);
    }
  }

  void ShowFailureSurface(
      const std::string &error,
      const std::string &log_path
  ) {
    if (smoke_test || !window || !IsWindow(window)) {
      return;
    }
    if (!failure_surface_) {
      failure_surface_ = std::make_unique<KSMWebViewLoadingSurface>();
      if (!failure_surface_->Create(
              window,
              ParentClientBounds(window),
              background_color,
              true,
              error_page_style_
          )) {
        failure_surface_.reset();
        return;
      }
    }
    failure_surface_->ShowFailure(ToWide(error), ToWide(log_path));
  }

  void ReportFailure(
      const std::string &error,
      const std::string &log_path
  ) {
    RecordFailure(error, log_path);
    ShowFailureSurface(error, log_path);
  }

  void RecordFailure(
      const std::string &error,
      const std::string &log_path
  ) {
    last_error_ = error;
    last_log_path_ = log_path;
  }

  void Fail(const std::string &message) {
    const std::wstring log_path = AppendHostLog(
        "ERROR",
        message + " | runtime=" + KSMWebView2Host::RuntimeVersion() +
            " | url=" + ToUtf8(start_url)
    );
    ReportFailure(message, ToUtf8(log_path));
    Complete(false, message);
    Close();
  }

  void Close() {
    if (closed) {
      return;
    }
    closed = true;
    std::vector<HWND> secondary_handles;
    for (const auto &[id, state] : secondary_windows_) {
      (void)id;
      if (state && state->window) {
        secondary_handles.push_back(state->window);
      }
    }
    secondary_windows_.clear();
    for (HWND secondary : secondary_handles) {
      if (secondary && IsWindow(secondary)) {
        SetWindowLongPtrW(secondary, GWLP_USERDATA, 0);
        DestroyWindow(secondary);
      }
    }
    for (const auto &[window_id, view] : views) {
      (void)window_id;
      view->Close();
    }
    views.clear();
    failed_views_.clear();
    default_view.reset();
    startup_surface_.reset();
    environment.Reset();
    environment_handler.Reset();
    if (!failure_surface_ && window && IsWindow(window)) {
      RemoveWindowSubclass(window, &Impl::SubclassProc,
                           reinterpret_cast<UINT_PTR>(this));
      window = nullptr;
    }
    if (!completed) {
      Complete(false, "The WebView2 session was closed.");
    }
  }

  static LRESULT CALLBACK SubclassProc(HWND window, UINT message, WPARAM wparam,
                                       LPARAM lparam, UINT_PTR,
                                       DWORD_PTR reference) {
    auto *self = reinterpret_cast<Impl *>(reference);
    if (message == WM_NCDESTROY) {
      RemoveWindowSubclass(window, &Impl::SubclassProc,
                           reinterpret_cast<UINT_PTR>(self));
      self->window = nullptr;
      self->Close();
    } else if (message == WM_WINDOWPOSCHANGING && !self->decorations) {
      auto *position = reinterpret_cast<WINDOWPOS *>(lparam);
      if (
          position &&
          (position->flags & SWP_NOSIZE) == 0 &&
          position->cx > 0 &&
          position->cy > 0
      ) {
        self->LayoutFillViewsForSize(position->cx, position->cy);
      }
    } else if (message == WM_SIZE) {
      if (self->startup_surface_) {
        self->startup_surface_->SetBounds(
            ParentClientBounds(window)
        );
      }
      if (self->failure_surface_) {
        self->failure_surface_->SetBounds(
            ParentClientBounds(window)
        );
      }
      self->LayoutFillViews();
    } else if (message == kRunUiTaskMessage) {
      self->RunHostUiTasks();
    } else if (
        message == WM_HOTKEY ||
        message == WM_APP + 0x4C
    ) {
      self->DispatchSystemMessage(
          message,
          static_cast<std::uintptr_t>(wparam)
      );
    }
    return DefSubclassProc(window, message, wparam, lparam);
  }

  HWND window = nullptr;
  std::shared_ptr<KSMFileSystem> file_system;
  std::vector<std::string> allowed_origins;
  std::vector<std::string> allowed_http_origins;
  std::filesystem::path web_root;
  std::filesystem::path user_data_folder;
  bool smoke_test = false;
  std::shared_ptr<KSMWebResourceProvider> web_resources;
  std::wstring start_url;
  bool dev_mode = false;
  std::string app_version;
  std::vector<std::string> allowed_commands;
  std::vector<std::string> allowed_permissions;
  std::vector<std::shared_ptr<KSMPlugin>> plugins;
  bool decorations = true;
  COREWEBVIEW2_COLOR background_color{255, 255, 255, 255};
  KSMWebViewErrorStyle error_page_style_;
  bool should_create_default_view = false;
  ComPtr<ICoreWebView2Environment> environment;
  ComPtr<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>
      environment_handler;
  std::unordered_map<std::uint64_t, std::shared_ptr<KSMWebView2View>> views;
  std::vector<std::shared_ptr<KSMWebView2View>> failed_views_;
  std::unordered_map<std::string, std::unique_ptr<SecondaryWindow>>
      secondary_windows_;
  std::shared_ptr<KSMWebView2View> default_view;
  std::shared_ptr<KSMWindowRegistry> window_registry_;
  std::unique_ptr<KSMWebViewLoadingSurface> startup_surface_;
  std::unique_ptr<KSMWebViewLoadingSurface> failure_surface_;
  std::string last_error_;
  std::string last_log_path_;
  DWORD ui_thread_id_ = 0;
  std::mutex host_ui_mutex_;
  std::deque<std::function<void()>> host_ui_tasks_;
  InitializationCallback completion;
  bool initializing = false;
  bool started = false;
  bool completed = false;
  bool closed = false;
};

KSMWebView2View::KSMWebView2View(std::shared_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

KSMWebView2View::~KSMWebView2View() = default;

bool KSMWebView2View::Initialize(InitializationCallback callback) {
  return impl_->Initialize(std::move(callback));
}

void KSMWebView2View::Close() { impl_->Close(); }

bool KSMWebView2View::SetBounds(const RECT &bounds) {
  return impl_->ApplyBounds(bounds, true);
}

bool KSMWebView2View::FillParent() { return impl_->FillParentBounds(); }

bool KSMWebView2View::SetVisible(bool visible) {
  return impl_->SetVisible(visible);
}

void KSMWebView2View::PrepareParentResize(int width, int height) {
  impl_->PrepareParentResize(width, height);
}

void KSMWebView2View::Focus() { impl_->FocusView(); }

bool KSMWebView2View::Navigate(const std::wstring &url) {
  return impl_->NavigateTo(url);
}

bool KSMWebView2View::Reload() { return impl_->ReloadPage(); }

HWND KSMWebView2View::window() const noexcept { return impl_->window; }

RECT KSMWebView2View::bounds() const noexcept { return impl_->desired_bounds; }

void KSMWebView2View::DispatchSystemMessage(
    unsigned int message,
    std::uintptr_t wparam
) {
    impl_->services->DispatchSystemMessage(message, wparam);
}

bool KSMWebView2View::is_initialized() const noexcept {
  return impl_->webview && !impl_->closed;
}

bool KSMWebView2View::is_visible() const noexcept { return impl_->visible; }

bool KSMWebView2View::fills_parent() const noexcept {
  return impl_->fill_parent;
}

std::uint64_t KSMWebView2View::window_id() const noexcept {
  return impl_->window_id_value;
}

KSMWebView2Host::KSMWebView2Host(
    HWND window, std::shared_ptr<KSMFileSystem> file_system,
    std::vector<std::string> allowed_external_origins,
    std::filesystem::path web_root, std::filesystem::path user_data_folder,
    bool smoke_test, std::shared_ptr<KSMWebResourceProvider> web_resources,
    std::wstring start_url, bool dev_mode, std::string app_version,
    std::vector<std::string> allowed_commands, bool decorations,
    std::vector<std::shared_ptr<KSMPlugin>> plugins,
    std::vector<std::string> allowed_permissions,
    std::vector<std::string> allowed_http_origins,
    std::string background_color,
    KSMWebView2ErrorPageOptions error_page)
    : impl_(std::make_shared<Impl>(
          window, std::move(file_system), std::move(allowed_external_origins),
          std::move(allowed_http_origins), std::move(web_root),
          std::move(user_data_folder), smoke_test,
          std::move(web_resources), std::move(start_url), dev_mode,
          std::move(app_version), std::move(allowed_commands),
          std::move(plugins), std::move(allowed_permissions),
          decorations, std::move(background_color),
          std::move(error_page))) {}

KSMWebView2Host::~KSMWebView2Host() = default;

bool KSMWebView2Host::InitializeSession(InitializationCallback callback) {
  return impl_->Start(false, std::move(callback));
}

bool KSMWebView2Host::Initialize(InitializationCallback callback) {
  return impl_->Start(true, std::move(callback));
}

std::shared_ptr<KSMWebView2View> KSMWebView2Host::CreateView(
    const KSMWebView2ViewOptions &options,
    KSMWebView2View::InitializationCallback callback) {
  return impl_->CreateView(options, std::move(callback));
}

bool KSMWebView2Host::DestroyView(std::uint64_t window_id) {
  return impl_->DestroyView(window_id);
}

void KSMWebView2Host::Close() { impl_->Close(); }

std::uint64_t KSMWebView2Host::window_id() const noexcept {
  return impl_->default_view ? impl_->default_view->window_id() : 0;
}

std::size_t KSMWebView2Host::view_count() const noexcept {
  return impl_->ViewCount();
}

std::string KSMWebView2Host::last_error() const {
  return impl_->last_error_;
}

std::string KSMWebView2Host::last_log_path() const {
  return impl_->last_log_path_;
}

bool KSMWebView2Host::IsRuntimeAvailable() {
  return !RuntimeVersionWide().empty();
}

std::string KSMWebView2Host::RuntimeVersion() {
  return ToUtf8(RuntimeVersionWide());
}
