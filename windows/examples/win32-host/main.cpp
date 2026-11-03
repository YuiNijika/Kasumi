#include <windows.h>

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <kasumi/commands/ksm_filesystem.h>
#include <kasumi/platform/ksm_window_frame.h>
#include <kasumi/platform/ksm_webview2_host.h>
#include <kasumi/platform/ksm_win32_resource_provider.h>

#if !defined(KASUMI_EXAMPLE_EXTERNAL_WEB)
#include "web_resources.h"
#endif

namespace {

constexpr wchar_t kWindowClass[] = L"KasumiExampleHostWindow";
constexpr wchar_t kWindowTitle[] = L"Kasumi Host Lab";
constexpr wchar_t kMultiViewSmokeProperty[] = L"KasumiMultiViewSmoke";
constexpr int kWindowWidth = 1080;
constexpr int kWindowHeight = 720;
constexpr UINT_PTR kMultiViewSmokeTimer = 0x4B4D;
constexpr UINT kMultiViewSmokeDelayMs = 250;
constexpr int kMultiViewSmokeMaxAttempts = 40;
#if defined(KASUMI_EXAMPLE_MULTI_VIEW)
constexpr bool kDefaultMultiView = KASUMI_EXAMPLE_MULTI_VIEW != 0;
#else
constexpr bool kDefaultMultiView = false;
#endif
#if defined(KASUMI_WINDOW_DECORATIONS)
constexpr bool kWindowDecorations = KASUMI_WINDOW_DECORATIONS != 0;
#else
constexpr bool kWindowDecorations = true;
#endif

struct MultiViewSmokeContext {
  KSMWebView2Host *host = nullptr;
  std::shared_ptr<KSMWebView2View> first;
  std::shared_ptr<KSMWebView2View> second;
  std::shared_ptr<KSMFileSystem> file_system;
  std::string marker;
  int ready_views = 0;
  int timer_attempts = 0;
  bool first_closed = false;
  bool split_layout = false;
};

std::wstring MultiViewPageUrl(const std::wstring &url, const wchar_t *pane) {
  const std::size_t query = url.find_first_of(L"?#");
  const std::size_t path_end = query == std::wstring::npos ? url.size() : query;
  const std::size_t slash = url.rfind(L'/', path_end);
  std::wstring result =
      slash == std::wstring::npos ? std::wstring() : url.substr(0, slash + 1);
  result += L"multi-view.html?pane=";
  result += pane;
  return result;
}

void LayoutMultiViews(MultiViewSmokeContext &context, HWND window) {
  if (!context.split_layout) {
    return;
  }

  RECT client{};
  if (!GetClientRect(window, &client)) {
    return;
  }
  const LONG width = client.right - client.left;
  const LONG height = client.bottom - client.top;
  if (width < 2 || height < 1) {
    return;
  }

  if (context.first && context.second) {
    const LONG split = client.left + width / 2;
    (void)context.first->SetBounds(
        RECT{client.left, client.top, split, client.bottom});
    (void)context.second->SetBounds(
        RECT{split, client.top, client.right, client.bottom});
    return;
  }
  if (context.first) {
    (void)context.first->FillParent();
  }
  if (context.second) {
    (void)context.second->FillParent();
  }
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

std::filesystem::path ExecutableDirectory() {
  wchar_t buffer[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(
      nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length == std::size(buffer)) {
    return std::filesystem::current_path();
  }
  return std::filesystem::path(buffer).parent_path();
}

std::optional<std::filesystem::path> LocalDataDirectory() {
  PWSTR known_folder = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                  nullptr, &known_folder)) ||
      known_folder == nullptr) {
    return std::nullopt;
  }
  std::filesystem::path result(known_folder);
  CoTaskMemFree(known_folder);
  return result / L"Kasumi" / L"Example";
}

std::optional<std::wstring> CommandLineValue(const std::wstring &flag) {
  int argument_count = 0;
  LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
  if (!arguments) {
    return std::nullopt;
  }
  std::optional<std::wstring> value;
  for (int index = 1; index < argument_count; ++index) {
    const std::wstring argument = arguments[index];
    if (argument == flag) {
      if (index + 1 < argument_count) {
        value = arguments[index + 1];
      }
      break;
    }
    if (argument.starts_with(flag + L"=")) {
      value = argument.substr(flag.size() + 1);
      break;
    }
  }
  LocalFree(arguments);
  return value;
}

bool HasCommandLineFlag(const wchar_t *flag) {
  int argument_count = 0;
  LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
  if (!arguments) {
    return false;
  }
  bool found = false;
  for (int index = 1; index < argument_count; ++index) {
    if (std::wstring(arguments[index]) == flag) {
      found = true;
      break;
    }
  }
  LocalFree(arguments);
  return found;
}

std::vector<std::string> ConfiguredValues(const std::string &configured) {
  std::vector<std::string> values;
  std::size_t start = 0;
  while (start < configured.size()) {
    const std::size_t separator = configured.find(',', start);
    const std::size_t end =
        separator == std::string::npos ? configured.size() : separator;
    if (end > start) {
      values.push_back(configured.substr(start, end - start));
    }
    if (separator == std::string::npos) {
      break;
    }
    start = separator + 1;
  }
  return values;
}

std::vector<std::string> ConfiguredCommands() {
  return ConfiguredValues(KASUMI_APP_COMMANDS);
}

std::vector<std::string> ConfiguredPermissions() {
  return ConfiguredValues(KASUMI_APP_PERMISSIONS);
}

std::vector<std::string> ConfiguredShellOrigins() {
  return ConfiguredValues(KASUMI_SHELL_ALLOWED_ORIGINS);
}

std::vector<std::string> ConfiguredHttpOrigins() {
  return ConfiguredValues(KASUMI_HTTP_ALLOWED_ORIGINS);
}

void ShowRuntimePrompt(HWND owner) {
  MessageBoxW(owner,
              L"Kasumi Host Lab needs the Microsoft Edge WebView2 Evergreen "
              L"Runtime.\n\n"
              L"Install it from "
              L"https://developer.microsoft.com/microsoft-edge/webview2/ "
              L"and start the example again.",
              L"WebView2 Runtime Required", MB_OK | MB_ICONWARNING);
}

void ReportSmokeFailure(const char *message) {
  std::fprintf(stderr, "Kasumi smoke test failed: %s\n", message);
  std::fflush(stderr);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                            LPARAM lparam) {
  switch (message) {
  case WM_TIMER:
    if (wparam == kMultiViewSmokeTimer) {
      auto *context = reinterpret_cast<MultiViewSmokeContext *>(
          GetPropW(window, kMultiViewSmokeProperty));
      if (!context) {
        KillTimer(window, kMultiViewSmokeTimer);
        PostQuitMessage(1);
        return 0;
      }

      if (!context->first_closed) {
        bool pages_reported = false;
        if (context->file_system && !context->marker.empty()) {
          const std::string directory = "app://data/multi-view-smoke";
          const std::string first_path =
              directory + "/" + context->marker + "-1.txt";
          const std::string second_path =
              directory + "/" + context->marker + "-2.txt";
          const auto first_content = context->file_system->ReadText(first_path);
          const auto second_content =
              context->file_system->ReadText(second_path);
          pages_reported = first_content == std::optional<std::string>("ready") &&
                           second_content == std::optional<std::string>("ready");
          if (pages_reported) {
            context->file_system->Remove(first_path);
            context->file_system->Remove(second_path);
            context->file_system->Remove(directory);
          }
        }
        if (!pages_reported &&
            ++context->timer_attempts < kMultiViewSmokeMaxAttempts) {
          return 0;
        }
        if (pages_reported && context->first && context->host &&
            context->host->DestroyView(context->first->window_id())) {
          context->first_closed = true;
          context->timer_attempts = 0;
          return 0;
        }
      }

      bool passed = false;
      if (context->first_closed) {
        RECT second_bounds{};
        bool resized_remaining_view = false;
        if (context->second && context->second->is_initialized()) {
          second_bounds = context->second->bounds();
          if (second_bounds.right - second_bounds.left > 1) {
            --second_bounds.right;
            resized_remaining_view = context->second->SetBounds(second_bounds);
          }
        }
        const bool has_views = context->first && context->second;
        const bool first_stopped =
            context->first && !context->first->is_initialized() &&
            !context->first->Reload();
        const bool second_running =
            context->second && context->second->is_initialized();
        const std::size_t view_count =
            context->host ? context->host->view_count() : 0;
        passed = has_views && context->ready_views == 2 && first_stopped &&
                 second_running && resized_remaining_view && view_count == 1;
        if (!passed) {
          std::fprintf(
              stderr,
              "multi-view smoke state: ready_views=%d has_views=%d "
              "first_stopped=%d second_running=%d resized_remaining=%d "
              "view_count=%zu\n",
              context->ready_views, has_views ? 1 : 0, first_stopped ? 1 : 0,
              second_running ? 1 : 0, resized_remaining_view ? 1 : 0,
              view_count);
          std::fflush(stderr);
        }
      }
      KillTimer(window, kMultiViewSmokeTimer);
      if (context->host) {
        context->host->Close();
      }
      PostQuitMessage(passed ? 0 : 1);
      return 0;
    }
    break;
  case WM_SIZE: {
    auto *context = reinterpret_cast<MultiViewSmokeContext *>(
        GetPropW(window, kMultiViewSmokeProperty));
    if (context) {
      LayoutMultiViews(*context, window);
    }
    break;
  }
  case WM_GETMINMAXINFO: {
    auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
    const bool split = GetPropW(window, kMultiViewSmokeProperty) != nullptr;
    info->ptMinTrackSize.x = split ? 640 : 320;
    info->ptMinTrackSize.y = split ? 360 : 240;
    return 0;
  }
  case WM_CLOSE:
    DestroyWindow(window);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  case WM_NCDESTROY:
    RemovePropW(window, kMultiViewSmokeProperty);
    break;
  default:
    break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
  const bool single_view_smoke = HasCommandLineFlag(L"--smoke-test");
  const bool multi_view = HasCommandLineFlag(L"--multi-view");
  const bool multi_view_smoke = HasCommandLineFlag(L"--multi-view-smoke");
  const bool smoke_test = single_view_smoke || multi_view_smoke;
  const bool use_multi_view =
      multi_view_smoke ||
      ((multi_view || kDefaultMultiView) && !single_view_smoke);
  const bool dev_mode = HasCommandLineFlag(L"--dev");
  const auto requested_url = CommandLineValue(L"--url");
  const std::wstring start_url =
      requested_url.value_or(L"https://app.local/index.html");
  const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
    return 1;
  }

  if (!KSMWebView2Host::IsRuntimeAvailable()) {
    if (!smoke_test) {
      ShowRuntimePrompt(nullptr);
    }
    if (SUCCEEDED(com_result)) {
      CoUninitialize();
    }
    return 1;
  }

  INITCOMMONCONTROLSEX controls{};
  controls.dwSize = sizeof(controls);
  controls.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&controls);

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.hInstance = instance;
  window_class.lpfnWndProc = WindowProc;
  window_class.lpszClassName = kWindowClass;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (!RegisterClassExW(&window_class)) {
    return 1;
  }

  HWND window =
      CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
                      nullptr, nullptr, instance, nullptr);
  if (!window) {
    return 1;
  }
  if (!KSMWindowFrame::Configure(window, kWindowDecorations)) {
    MessageBoxW(window, L"The application window frame could not be configured.",
                kWindowTitle, MB_OK | MB_ICONERROR);
    DestroyWindow(window);
    return 1;
  }

  const auto executable_dir = ExecutableDirectory();
  std::filesystem::path web_root;
  std::vector<KSMWin32ResourceDescriptor> resource_descriptors;
  std::shared_ptr<KSMWebResourceProvider> web_resources;
  const auto requested_web_root = CommandLineValue(L"--web-root");
#if defined(KASUMI_EXAMPLE_EXTERNAL_WEB)
  web_root = requested_web_root.value_or(executable_dir / L"web");
#else
  if (requested_web_root) {
    web_root = *requested_web_root;
  }
#endif
  if (!web_root.empty() &&
      !std::filesystem::is_regular_file(web_root / L"index.html")) {
    MessageBoxW(window, L"The example web root does not contain index.html.",
                L"Kasumi Host Lab", MB_OK | MB_ICONERROR);
    DestroyWindow(window);
    return 1;
  }

  const auto local_data = LocalDataDirectory();
  if (!local_data) {
    MessageBoxW(window, L"The local application data directory is unavailable.",
                L"Kasumi Host Lab", MB_OK | MB_ICONERROR);
    DestroyWindow(window);
    return 1;
  }
  const auto requested_data = CommandLineValue(L"--user-data-folder");
  std::filesystem::path webview_data;
  if (requested_data) {
    webview_data = *requested_data;
  } else if (smoke_test) {
    webview_data =
        *local_data / L"WebView2Smoke" / std::to_wstring(GetCurrentProcessId());
  } else {
    webview_data = *local_data / L"WebView2";
  }
  std::shared_ptr<KSMFileSystem> file_system;
#if defined(KASUMI_EXAMPLE_EXTERNAL_WEB)
  file_system = std::make_shared<KSMFileSystem>(
      *local_data / L"data", *local_data / L"cache", *local_data / L"tmp",
      web_root / L"res", web_root / L"assets");
#else
  if (web_root.empty()) {
    resource_descriptors = kasumi_example::embedded::Resources();
    web_resources = std::make_shared<KSMWin32ResourceProvider>(
        instance, resource_descriptors);
  }
  if (web_resources) {
    file_system = std::make_shared<KSMFileSystem>(
        *local_data / L"data", *local_data / L"cache", *local_data / L"tmp",
        std::filesystem::path(), std::filesystem::path(),
        [web_resources](bool is_asset, const std::string &path) {
          const std::string prefix = is_asset ? "assets/" : "res/";
          const auto resource = web_resources->Read(prefix + path);
          if (!resource) {
            return std::optional<KSMFileSystem::ReadOnlyResource>();
          }
          return std::optional<KSMFileSystem::ReadOnlyResource>(
              KSMFileSystem::ReadOnlyResource{resource->mime_type,
                                              resource->data});
        },
        [web_resources](bool is_asset, const std::string &path) {
          const std::string prefix = is_asset ? "assets/" : "res/";
          return web_resources->List(prefix + path);
        });
  } else {
    file_system = std::make_shared<KSMFileSystem>(
        *local_data / L"data", *local_data / L"cache", *local_data / L"tmp",
        web_root / L"res", web_root / L"assets");
  }
#endif
  ShowWindow(window, smoke_test ? SW_SHOWNOACTIVATE : show_command);
  UpdateWindow(window);

 auto host = std::unique_ptr<KSMWebView2Host>(new KSMWebView2Host(
 window, file_system, ConfiguredShellOrigins(),
 web_root, webview_data, smoke_test, web_resources, start_url, dev_mode,
 KASUMI_APP_VERSION, ConfiguredCommands(), kWindowDecorations,
 std::vector<std::shared_ptr<KSMPlugin>>{},
 ConfiguredPermissions(), ConfiguredHttpOrigins()));

  MultiViewSmokeContext multi_view_context;
  multi_view_context.host = host.get();
  multi_view_context.file_system = file_system;
  multi_view_context.split_layout = use_multi_view;
  if (multi_view_smoke) {
    multi_view_context.marker = std::to_string(GetCurrentProcessId());
  }
  if (use_multi_view) {
    SetPropW(window, kMultiViewSmokeProperty, &multi_view_context);
  }

  const auto show_error = [window, smoke_test](const std::string &error) {
    if (smoke_test) {
      ReportSmokeFailure(error.c_str());
      PostQuitMessage(1);
      return;
    }
    const std::wstring message = ToWide(error);
    MessageBoxW(window, message.c_str(), L"Kasumi Host Lab",
                MB_OK | MB_ICONERROR);
    PostMessageW(window, WM_CLOSE, 0, 0);
  };

  if (!use_multi_view) {
    if (!host->Initialize(
            [window, smoke_test](bool success, const std::string &error) {
              if (success) {
                if (smoke_test) {
                  ShowWindow(window, SW_HIDE);
                }
                return;
              }
              if (smoke_test) {
                PostQuitMessage(1);
                return;
              }
              const std::wstring message = ToWide(error);
              MessageBoxW(window, message.c_str(), L"Kasumi Host Lab",
                          MB_OK | MB_ICONERROR);
              PostMessageW(window, WM_CLOSE, 0, 0);
            })) {
      host.reset();
      DestroyWindow(window);
      return 1;
    }
  } else {
    const bool initialized = host->InitializeSession(
        [&, window, multi_view_smoke, show_error](
            bool success, const std::string &error) {
          if (!success) {
            show_error(error);
            return;
          }

          RECT client{};
          GetClientRect(window, &client);
          const LONG width = client.right - client.left;
          const LONG height = client.bottom - client.top;
          const LONG split = width / 2;
          if (width < 2 || height < 1) {
            show_error("The multi-view host window is too small.");
            return;
          }

          KSMWebView2ViewOptions empty_options;
          empty_options.bounds =
              RECT{client.left, client.top, client.left, client.top};
          empty_options.fill_parent = false;
          if (host->CreateView(empty_options)) {
            show_error("The multi-view host created a view with empty bounds.");
            return;
          }

          auto view_ready = [&, window, multi_view_smoke, show_error](
                                bool view_success,
                                const std::string &view_error) {
            if (!view_success) {
              show_error(view_error);
              return;
            }
            ++multi_view_context.ready_views;
            if (multi_view_context.ready_views != 2) {
              return;
            }
            if (!multi_view_context.first || !multi_view_context.second) {
              show_error("The multi-view host did not retain both views.");
              return;
            }

            const RECT first_bounds = multi_view_context.first->bounds();
            const RECT empty_bounds{
                first_bounds.left, first_bounds.top, first_bounds.left,
                first_bounds.top};
            if (multi_view_context.first->SetBounds(empty_bounds)) {
              show_error("The multi-view host accepted an empty view bounds.");
              return;
            }
            const RECT preserved_bounds = multi_view_context.first->bounds();
            if (preserved_bounds.left != first_bounds.left ||
                preserved_bounds.top != first_bounds.top ||
                preserved_bounds.right != first_bounds.right ||
                preserved_bounds.bottom != first_bounds.bottom) {
              show_error("Rejected view bounds changed the existing layout.");
              return;
            }

            const bool ids_differ =
                multi_view_context.first->window_id() !=
                multi_view_context.second->window_id();
            const bool windows_differ =
                multi_view_context.first->window() !=
                multi_view_context.second->window();
            const RECT second_bounds = multi_view_context.second->bounds();
            const bool bounds_differ =
                first_bounds.left != second_bounds.left ||
                first_bounds.right != second_bounds.right;
            if (!ids_differ || !windows_differ || !bounds_differ ||
                host->view_count() != 2) {
              show_error(
                  "The multi-view host did not isolate view identity or bounds.");
              return;
            }

            if (!multi_view_smoke) {
              multi_view_context.first->Focus();
              return;
            }

            RECT window_bounds{};
            if (!GetWindowRect(window, &window_bounds)) {
              show_error("The multi-view smoke window bounds are unavailable.");
              return;
            }
            SetWindowPos(
                window, nullptr, window_bounds.left, window_bounds.top,
                window_bounds.right - window_bounds.left + 80,
                window_bounds.bottom - window_bounds.top + 40,
                SWP_NOZORDER | SWP_NOACTIVATE);
            RECT resized_client{};
            const RECT resized_first =
                multi_view_context.first->bounds();
            const RECT resized_second =
                multi_view_context.second->bounds();
            if (!GetClientRect(window, &resized_client)) {
              show_error("The resized multi-view client area is unavailable.");
              return;
            }
            const LONG expected_split =
                resized_client.left +
                (resized_client.right - resized_client.left) / 2;
            if (resized_first.left != resized_client.left ||
                resized_first.right != expected_split ||
                resized_second.left != expected_split ||
                resized_second.right != resized_client.right) {
              show_error(
                  "The multi-view host did not relayout after a window resize.");
              return;
            }

            if (SetTimer(window, kMultiViewSmokeTimer,
                         kMultiViewSmokeDelayMs, nullptr) == 0) {
              show_error("The multi-view smoke timer could not be created.");
            }
          };

          KSMWebView2ViewOptions first_options;
          first_options.start_url = MultiViewPageUrl(start_url, L"left");
          if (multi_view_smoke) {
            const std::wstring marker(
                multi_view_context.marker.begin(),
                multi_view_context.marker.end());
            first_options.start_url +=
                first_options.start_url.find(L'?') == std::wstring::npos
                    ? L"?multi-view-smoke="
                    : L"&multi-view-smoke=";
            first_options.start_url += marker + L"&view=1";
          }
          first_options.bounds = RECT{client.left, client.top, split,
                                      client.bottom};
          first_options.fill_parent = false;
          first_options.append_smoke_query = false;
          first_options.allowed_commands = ConfiguredCommands();
          first_options.allowed_permissions = ConfiguredPermissions();
          multi_view_context.first =
              host->CreateView(first_options, view_ready);

          KSMWebView2ViewOptions second_options = first_options;
          if (multi_view_smoke) {
            const std::wstring marker(
                multi_view_context.marker.begin(),
                multi_view_context.marker.end());
            second_options.start_url =
                MultiViewPageUrl(start_url, L"right");
            second_options.start_url += L"&multi-view-smoke=";
            second_options.start_url += marker + L"&view=2";
          } else {
            second_options.start_url =
                MultiViewPageUrl(start_url, L"right");
          }
          second_options.bounds =
              RECT{split, client.top, client.right, client.bottom};
          multi_view_context.second =
              host->CreateView(second_options, view_ready);
          if (!multi_view_context.first || !multi_view_context.second) {
            show_error("The multi-view host could not create both WebViews.");
          }
        });
    if (!initialized) {
      host.reset();
      DestroyWindow(window);
      return 1;
    }
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  host->Close();
  host.reset();
  multi_view_context.first.reset();
  multi_view_context.second.reset();
  const int result = static_cast<int>(message.wParam);
  if (SUCCEEDED(com_result)) {
    CoUninitialize();
  }
  return result;
}
