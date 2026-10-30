#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>

#include <filesystem>
#include <iterator>
#include <memory>
#include <string>

#include "../../src/commands/ksm_filesystem.h"
#include "../../src/platform/ksm_webview2_host.h"

namespace {

constexpr wchar_t kWindowClass[] = L"KasumiExampleHostWindow";
constexpr wchar_t kWindowTitle[] = L"Kasumi Host Lab";
constexpr int kWindowWidth = 1080;
constexpr int kWindowHeight = 720;

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

std::filesystem::path ExecutableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer,
        static_cast<DWORD>(std::size(buffer))
    );
    if (length == 0 || length == std::size(buffer)) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path LocalDataDirectory() {
    PWSTR known_folder = nullptr;
    if (
        FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_CREATE,
            nullptr,
            &known_folder
        )) ||
        known_folder == nullptr
    ) {
        return ExecutableDirectory() / L"KasumiData";
    }
    std::filesystem::path result(known_folder);
    CoTaskMemFree(known_folder);
    return result / L"Kasumi" / L"Example";
}

bool HasCommandLineFlag(const wchar_t* flag) {
    int argument_count = 0;
    LPWSTR* arguments = CommandLineToArgvW(
        GetCommandLineW(),
        &argument_count
    );
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

void ShowRuntimePrompt(HWND owner) {
    MessageBoxW(
        owner,
        L"Kasumi Host Lab needs the Microsoft Edge WebView2 Evergreen Runtime.\n\n"
        L"Install it from https://developer.microsoft.com/microsoft-edge/webview2/ "
        L"and start the example again.",
        L"WebView2 Runtime Required",
        MB_OK | MB_ICONWARNING
    );
}

LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = 320;
        info->ptMinTrackSize.y = 240;
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show_command
) {
    const bool smoke_test = HasCommandLineFlag(L"--smoke-test");
    const HRESULT com_result = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED
    );
    if (
        FAILED(com_result) &&
        com_result != RPC_E_CHANGED_MODE
    ) {
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

    HWND window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
    if (!window) {
        return 1;
    }

    const auto executable_dir = ExecutableDirectory();
    const auto web_root = executable_dir / L"web";
    if (!std::filesystem::is_regular_file(web_root / L"index.html")) {
        MessageBoxW(
            window,
            L"The example web assets are missing. Keep the web folder next to "
            L"the executable.",
            L"Kasumi Host Lab",
            MB_OK | MB_ICONERROR
        );
        DestroyWindow(window);
        return 1;
    }

    const auto local_data = LocalDataDirectory();
    const auto file_system = std::make_shared<KSMFileSystem>(
        local_data / L"data",
        local_data / L"cache",
        local_data / L"tmp",
        web_root / L"res",
        web_root / L"assets"
    );
    auto host = std::make_unique<KSMWebView2Host>(
        window,
        file_system,
        std::vector<std::string>{"https://example.com"},
        web_root,
        local_data / L"WebView2",
        smoke_test
    );

    if (!host->Initialize([window](bool success, const std::string& error) {
        if (success) {
            return;
        }
        const std::wstring message = ToWide(error);
        MessageBoxW(
            window,
            message.c_str(),
            L"Kasumi Host Lab",
            MB_OK | MB_ICONERROR
        );
        PostMessageW(window, WM_CLOSE, 0, 0);
    })) {
        host.reset();
        DestroyWindow(window);
        return 1;
    }

    ShowWindow(window, smoke_test ? SW_HIDE : show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    host->Close();
    host.reset();
    const int result = static_cast<int>(message.wParam);
    if (SUCCEEDED(com_result)) {
        CoUninitialize();
    }
    return result;
}
