#include "ksm_webview2_host.h"

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

#include "../bridge/ksm_router.h"
#include "ksm_host_services.h"
#include "ksm_web_resource.h"
#include "ksm_webview_host.h"
#include "ksm_webview_security.h"
#include "ksm_windows_host_services.h"

namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

constexpr UINT kRunUiTaskMessage = WM_APP + 0x4B;

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

} // namespace

struct KSMWebView2Host::Impl {
  Impl(HWND window, std::shared_ptr<KSMFileSystem> file_system,
       std::vector<std::string> allowed_origins, std::filesystem::path web_root,
       std::filesystem::path user_data_folder, bool smoke_test,
       std::shared_ptr<KSMWebResourceProvider> web_resources,
       std::wstring start_url, bool dev_mode)
      : window(window), window_id_value(++next_window_id),
        web_root(std::move(web_root)),
        user_data_folder(std::move(user_data_folder)), smoke_test(smoke_test),
        web_resources(std::move(web_resources)),
        start_url(std::move(start_url)), dev_mode(dev_mode),
        services(std::make_shared<KSMWindowsHostServices>(
            window, std::move(allowed_origins), file_system)),
        webview_host(
            [this](const std::string &message) {
              PostToUi([this, message] {
                if (webview && !closed) {
                  const std::wstring wide_message = ToWide(message);
                  webview->PostWebMessageAsString(wide_message.c_str());
                }
              });
            },
            file_system, services,
            [this](std::function<void()> task) { PostToUi(std::move(task)); }) {
    SetWindowSubclass(window, &Impl::SubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(this));
  }

  ~Impl() {
    Close();
    RemoveWindowSubclass(window, &Impl::SubclassProc,
                         reinterpret_cast<UINT_PTR>(this));
  }

  bool Initialize(InitializationCallback callback) {
    if (closed || initializing) {
      return false;
    }
    initializing = true;
    completion = std::move(callback);
    if (!IsRuntimeAvailable()) {
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

    controller_handler_ =
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result,
                   ICoreWebView2Controller *controller) -> HRESULT {
              if (closed) {
                return S_OK;
              }
              if (FAILED(result) || controller == nullptr) {
                Fail("The WebView2 controller could not be created.");
                return S_OK;
              }
              controller_ = controller;
              controller_->get_CoreWebView2(&webview);
              if (!webview) {
                Fail("The WebView2 instance could not be created.");
                return S_OK;
              }
              ConfigureWebView();
              NavigateToStartPage();
              Complete(true, {});
              return S_OK;
            });

    environment_handler_ =
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result,
                   ICoreWebView2Environment *environment) -> HRESULT {
              if (closed) {
                return S_OK;
              }
              if (FAILED(result) || environment == nullptr) {
                Fail("The WebView2 environment could not be created.");
                return S_OK;
              }
              environment_ = environment;
              PostToUi([this] {
                if (closed) {
                  return;
                }
                const HRESULT controller_result =
                    environment_->CreateCoreWebView2Controller(
                        window, controller_handler_.Get());
                if (FAILED(controller_result)) {
                  Fail("The WebView2 controller could not be created.");
                }
              });
              return S_OK;
            });

    ComPtr<CoreWebView2EnvironmentOptions> options;
    if (smoke_test) {
      options = Make<CoreWebView2EnvironmentOptions>();
      // CI desktop sessions can block Chromium's sandbox before the
      // controller callback runs.
      options->put_AdditionalBrowserArguments(L"--no-sandbox");
    }
    const std::wstring data_folder = user_data_folder.wstring();
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, data_folder.empty() ? nullptr : data_folder.c_str(),
        options.Get(), environment_handler_.Get());
    if (FAILED(result)) {
      Fail("WebView2 initialization failed.");
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

    webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2 *,
                   ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
              LPWSTR uri = nullptr;
              if (FAILED(args->get_Uri(&uri)) || uri == nullptr) {
                return S_OK;
              }
              const std::string url = ToUtf8(uri);
              CoTaskMemFree(uri);
              if (!KSMWebViewSecurity::IsAllowedNavigation(url)) {
                args->put_Cancel(TRUE);
              }
              return S_OK;
            })
            .Get(),
        &navigation_starting_token);
    webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this](ICoreWebView2 *,
                   ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
              args->put_Handled(TRUE);
              return S_OK;
            })
            .Get(),
        &new_window_token);
    webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2 *,
                   ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
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
              webview_host.OnMessageReceivedFromOrigin(ToUtf8(message),
                                                       ToUtf8(source));
              CoTaskMemFree(source);
              CoTaskMemFree(message);
              return S_OK;
            })
            .Get(),
        &message_token);
    webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2 *,
                   ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
              BOOL success = FALSE;
              if (SUCCEEDED(args->get_IsSuccess(&success)) && success) {
                webview_host.NotifyWebViewReloaded();
              }
              return S_OK;
            })
            .Get(),
        &navigation_completed_token);
    Resize();
  }

  void ConfigureEmbeddedResources() {
    webview->AddWebResourceRequestedFilter(
        L"https://app.local/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](
                ICoreWebView2 *,
                ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
              if (!args || !environment_ || !web_resources) {
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
                  path ? web_resources->Read(*path) : std::nullopt;
              if (!resource) {
                ComPtr<ICoreWebView2WebResourceResponse> response;
                if (SUCCEEDED(environment_->CreateWebResourceResponse(
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
              if (SUCCEEDED(environment_->CreateWebResourceResponse(
                      content.Get(), 200, L"OK", headers.c_str(), &response))) {
                args->put_Response(response.Get());
              }
              return S_OK;
            })
            .Get(),
        &web_resource_token);
  }

  void NavigateToStartPage() {
    if (webview) {
      std::wstring url = start_url;
      if (smoke_test) {
        url += url.find(L'?') == std::wstring::npos ? L"?smoke=1" : L"&smoke=1";
      }
      webview->Navigate(url.c_str());
    }
  }

  void Resize() {
    if (!controller_) {
      return;
    }
    RECT bounds{};
    if (!GetClientRect(window, &bounds)) {
      return;
    }
    controller_->put_Bounds(bounds);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width > 0 && height > 0) {
      webview_host.NotifyWindowResized(width, height);
    }
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
    {
      std::lock_guard lock(ui_mutex);
      if (closed) {
        return;
      }
      ui_tasks.push_back(std::move(task));
    }
    PostMessageW(window, kRunUiTaskMessage, 0, 0);
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

  void Fail(const std::string &message) { Complete(false, message); }

  void Close() {
    if (closed) {
      return;
    }
    {
      std::lock_guard lock(ui_mutex);
      closed = true;
      ui_tasks.clear();
    }
    if (webview && message_token.value != 0) {
      webview->remove_WebMessageReceived(message_token);
    }
    if (webview && navigation_starting_token.value != 0) {
      webview->remove_NavigationStarting(navigation_starting_token);
    }
    if (webview && new_window_token.value != 0) {
      webview->remove_NewWindowRequested(new_window_token);
    }
    if (webview && navigation_completed_token.value != 0) {
      webview->remove_NavigationCompleted(navigation_completed_token);
    }
    if (webview && web_resource_token.value != 0) {
      webview->remove_WebResourceRequested(web_resource_token);
    }
    if (webview && !web_root.empty()) {
      ComPtr<ICoreWebView2_3> webview3;
      if (SUCCEEDED(webview.As(&webview3)) && webview3) {
        webview3->ClearVirtualHostNameToFolderMapping(L"app.local");
      }
    }
    webview.Reset();
    if (controller_) {
      controller_->Close();
      controller_.Reset();
    }
    environment_.Reset();
    controller_handler_.Reset();
    environment_handler_.Reset();
  }

  static LRESULT CALLBACK SubclassProc(HWND window, UINT message, WPARAM wparam,
                                       LPARAM lparam, UINT_PTR,
                                       DWORD_PTR reference) {
    auto *self = reinterpret_cast<Impl *>(reference);
    if (message == kRunUiTaskMessage) {
      self->RunUiTasks();
      return 0;
    }
    if (message == WM_SIZE) {
      self->Resize();
    }
    if (message == WM_NCDESTROY) {
      RemoveWindowSubclass(window, &Impl::SubclassProc,
                           reinterpret_cast<UINT_PTR>(self));
    }
    return DefSubclassProc(window, message, wparam, lparam);
  }

  static std::atomic<std::uint64_t> next_window_id;

  HWND window;
  std::uint64_t window_id_value;
  std::filesystem::path web_root;
  std::filesystem::path user_data_folder;
  bool smoke_test;
  std::shared_ptr<KSMWebResourceProvider> web_resources;
  std::wstring start_url;
  bool dev_mode;
  std::shared_ptr<KSMWindowsHostServices> services;
  KSMWebViewHost webview_host;
  ComPtr<ICoreWebView2Environment> environment_;
  ComPtr<ICoreWebView2Controller> controller_;
  ComPtr<ICoreWebView2> webview;
  ComPtr<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>
      environment_handler_;
  ComPtr<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>
      controller_handler_;
  EventRegistrationToken navigation_starting_token{};
  EventRegistrationToken new_window_token{};
  EventRegistrationToken message_token{};
  EventRegistrationToken navigation_completed_token{};
  EventRegistrationToken web_resource_token{};
  std::mutex ui_mutex;
  std::deque<std::function<void()>> ui_tasks;
  InitializationCallback completion;
  bool initializing = false;
  bool completed = false;
  bool closed = false;
};

std::atomic<std::uint64_t> KSMWebView2Host::Impl::next_window_id{1};

KSMWebView2Host::KSMWebView2Host(
    HWND window, std::shared_ptr<KSMFileSystem> file_system,
    std::vector<std::string> allowed_external_origins,
    std::filesystem::path web_root, std::filesystem::path user_data_folder,
    bool smoke_test, std::shared_ptr<KSMWebResourceProvider> web_resources,
    std::wstring start_url, bool dev_mode)
    : impl_(std::make_unique<Impl>(
          window, std::move(file_system), std::move(allowed_external_origins),
          std::move(web_root), std::move(user_data_folder), smoke_test,
          std::move(web_resources), std::move(start_url), dev_mode)) {}

KSMWebView2Host::~KSMWebView2Host() = default;

bool KSMWebView2Host::Initialize(InitializationCallback callback) {
  return impl_->Initialize(std::move(callback));
}

void KSMWebView2Host::Close() { impl_->Close(); }

std::uint64_t KSMWebView2Host::window_id() const noexcept {
  return impl_->window_id_value;
}

bool KSMWebView2Host::IsRuntimeAvailable() {
  return !RuntimeVersionWide().empty();
}

std::string KSMWebView2Host::RuntimeVersion() {
  return ToUtf8(RuntimeVersionWide());
}
