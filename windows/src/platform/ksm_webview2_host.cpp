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
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../bridge/ksm_router.h"
#include "ksm_host_services.h"
#include "ksm_web_resource.h"
#include "ksm_webview_host.h"
#include "ksm_webview_security.h"
#include "ksm_window_frame.h"
#include "ksm_windows_host_services.h"

namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

constexpr UINT kRunUiTaskMessage = WM_APP + 0x4B;
constexpr wchar_t kViewWindowClass[] = L"KasumiWebView2View";

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

} // namespace

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
       KSMWebView2ViewOptions options)
      : parent(parent), window_id_value(++next_window_id),
        file_system(std::move(file_system)),
        allowed_origins(std::move(allowed_origins)),
        allowed_http_origins(std::move(allowed_http_origins)),
        environment(std::move(environment)), web_root(std::move(web_root)),
        smoke_test(smoke_test), web_resources(std::move(web_resources)),
        start_url(std::move(start_url)), dev_mode(dev_mode),
        decorations(decorations), fill_parent(options.fill_parent),
        visible(true),
        services(std::make_shared<KSMWindowsHostServices>(
            parent, this->allowed_origins, this->allowed_http_origins,
            this->file_system, std::move(app_version))),
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
              if (SUCCEEDED(args->get_IsSuccess(&success)) && success) {
                const bool is_reload =
                    self->pending_navigation_kind ==
                    COREWEBVIEW2_NAVIGATION_KIND_RELOAD;
                self->pending_navigation_kind =
                    COREWEBVIEW2_NAVIGATION_KIND_NEW_DOCUMENT;
                self->webview_host.NotifyWebViewNavigated(is_reload);
              }
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
                    std::string(KSMWebViewSecurity::ContentSecurityPolicy()));
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
      return;
    }
    std::wstring url = start_url;
    if (smoke_test && append_smoke_query) {
      url += url.find(L'?') == std::wstring::npos ? L"?smoke=1" : L"&smoke=1";
    }
    webview->Navigate(url.c_str());
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

  bool SetVisible(bool next_visible) {
    if (closed) {
      return false;
    }
    visible = next_visible;
    if (window) {
      ShowWindow(window, visible ? SW_SHOWNA : SW_HIDE);
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
    if (!completed) {
      Complete(false, "The WebView2 view was destroyed.");
    }
  }

  void Close() {
    if (closed && !window) {
      return;
    }
    closed = true;
    {
      std::lock_guard lock(ui_mutex);
      ui_tasks.clear();
    }
    ReleaseWebView();
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
       std::vector<std::string> allowed_permissions, bool decorations)
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
 decorations(decorations) {
    SetWindowSubclass(window, &Impl::SubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(this));
  }

  ~Impl() { Close(); }

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
    initializing = true;
    started = true;
    should_create_default_view = create_default_view;
    completion = std::move(completion_callback);
    if (!KSMWebView2Host::IsRuntimeAvailable()) {
      Fail("The Evergreen WebView2 Runtime is not installed.");
      return false;
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

    ComPtr<CoreWebView2EnvironmentOptions> options;
#if defined(KASUMI_ENABLE_SMOKE_TEST) && KASUMI_ENABLE_SMOKE_TEST
    if (smoke_test) {
      options = Make<CoreWebView2EnvironmentOptions>();
      // CI desktop sessions can block Chromium's sandbox before the
      // controller callback runs.
      options->put_AdditionalBrowserArguments(L"--no-sandbox");
    }
#endif
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
      const KSMWebView2ViewOptions &options) {
    if (closed || !environment) {
      return nullptr;
    }
    return std::make_shared<KSMWebView2View::Impl>(
        window, file_system, allowed_origins, allowed_http_origins, environment,
        web_root, smoke_test, web_resources, start_url, dev_mode, app_version,
        options.allowed_commands.empty() ? allowed_commands
                                         : options.allowed_commands,
        plugins,
        options.allowed_permissions.empty() ? allowed_permissions
                                            : options.allowed_permissions,
        decorations, options);
  }

  std::shared_ptr<KSMWebView2View> CreateView(
      const KSMWebView2ViewOptions &options,
      KSMWebView2View::InitializationCallback callback) {
    if (closed || !environment) {
      if (callback) {
        callback(false, "The WebView2 session is not initialized.");
      }
      return nullptr;
    }
    auto state = CreateViewState(options);
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

  void Fail(const std::string &message) {
    Complete(false, message);
    Close();
  }

  void Close() {
    if (closed) {
      return;
    }
    closed = true;
    for (const auto &[window_id, view] : views) {
      (void)window_id;
      view->Close();
    }
    views.clear();
    default_view.reset();
    environment.Reset();
    environment_handler.Reset();
    if (window && IsWindow(window)) {
      RemoveWindowSubclass(window, &Impl::SubclassProc,
                           reinterpret_cast<UINT_PTR>(this));
    }
    window = nullptr;
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
    } else if (message == WM_SIZE) {
      self->LayoutFillViews();
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
  bool should_create_default_view = false;
  ComPtr<ICoreWebView2Environment> environment;
  ComPtr<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>
      environment_handler;
  std::unordered_map<std::uint64_t, std::shared_ptr<KSMWebView2View>> views;
  std::shared_ptr<KSMWebView2View> default_view;
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
    std::vector<std::string> allowed_http_origins)
    : impl_(std::make_shared<Impl>(
          window, std::move(file_system), std::move(allowed_external_origins),
          std::move(allowed_http_origins), std::move(web_root),
          std::move(user_data_folder), smoke_test,
          std::move(web_resources), std::move(start_url), dev_mode,
          std::move(app_version), std::move(allowed_commands),
          std::move(plugins), std::move(allowed_permissions),
          decorations)) {}

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

bool KSMWebView2Host::IsRuntimeAvailable() {
  return !RuntimeVersionWide().empty();
}

std::string KSMWebView2Host::RuntimeVersion() {
  return ToUtf8(RuntimeVersionWide());
}
