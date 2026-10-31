# Windows SDK

> 说明 Windows 侧的静态库、公共头文件、WebView2 适配、独立示例、打包和发布签名。

## 模块边界

`windows/` 是 SDK 构建根目录，只产出静态库、公共头文件和 CMake 包配置。可运行示例
位于独立工程 `windows/examples/win32-host/`，通过已安装的 CMake 包消费 SDK。

| 路径 | 责任 |
| --- | --- |
| `src/commands/ksm_phase1.h` | 宿主侧 `KSM` 根入口、生命周期和统一调用 |
| `src/bridge/ksm_command_registry.h` | Windows 已实现命令清单 |
| `src/bridge/ksm_router.h` | 参数校验、权限检查和命令分发 |
| `src/platform/ksm_host_services.h` | 宿主平台服务接口 |
| `src/platform/ksm_windows_host_services.h` | 文件、对话框、剪贴板、通知、下载、链接和窗口服务 |
| `src/platform/ksm_web_resource.h` | 内嵌 Web 资源提供器接口和 URL 到资源路径解析 |
| `src/platform/ksm_win32_resource_provider.h` | 从 Win32 `RCDATA` 读取页面、样式和脚本 |
| `src/platform/ksm_webview_host.h` | 工作线程、超时、取消、单响应和事件投递 |
| `src/platform/ksm_webview2_host.h` | WebView2 创建、内嵌资源响应、来源校验、导航拦截和 UI 线程回投 |
| `tests/contract_runner.cpp` | Windows 契约、安全策略和生命周期测试 |
| `examples/win32-host/` | 独立 Win32 + WebView2 示例工程 |

Windows 当前实现 **28/28 个适用命令**。`device.getInfo` 是 Android 专属命令，
Windows 返回 `NOT_SUPPORTED`。

## 宿主入口

```cpp
#include <kasumi/commands/ksm_filesystem.h>
#include <kasumi/platform/ksm_webview2_host.h>

KSM::Init(file_system, host_services);
const std::string response = KSM::Invoke(
    "window-1",
    "http.download",
    R"({"url":"https://example.com/file.bin","path":"app://data/file.bin"})"
);
```

`KSM::Invoke(windowId, ...)` 按窗口维护独立路由器和事件订阅。空 `windowId` 使用默认
路由上下文。宿主根入口直接调用 `KSMHostServices`，不得通过 `KSM::Invoke` 递归回路由。

| 子模块 | 入口 | 宿主服务 |
| --- | --- | --- |
| 文件系统 | `KSM::Fs` | `KSMFileSystem` 与 picked 文件方法 |
| 对话框 | `KSM::Dialog` | 对话框与文件选择方法 |
| 剪贴板 | `KSM::Clipboard` | 读写系统剪贴板 |
| 通知 | `KSM::Notification` | `ShowNotification` |
| 权限 | `KSM::Permission` | 生成的权限清单 |
| 窗口 | `KSM::Window` | 标题、最小化、最大化、尺寸和关闭 |
| HTTP | `KSM::Http` | `Download` |
| Shell | `KSM::Shell` | `OpenExternal` |
| 设备 | `KSM::Device` | `GetInfo`，Windows 返回空 |
| 应用 | `KSM::App` | 版本、语言和退出 |

## SDK 构建

```powershell
pnpm ksm build --windows --target sdk
python cli/ksm.py build --platform windows --target sdk
```

等价的 CMake 命令：

```powershell
cmake -S windows -B windows/build/sdk `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="$PWD/windows/build/sdk/install" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build windows/build/sdk --config Release --target install
```

安装目录包含：

```text
windows/build/sdk/install/
├─ include/kasumi/
├─ lib/kasumi_windows_bridge.lib
├─ lib/cmake/Kasumi/
└─ README.md
```

消费者只链接导入目标 `Kasumi::kasumi_windows_bridge`，不编译 `windows/src/` 下的
C++ 实现文件。依赖通过 `vcpkg.json` 固定为 `nlohmann-json` 与 `webview2`。
默认构建使用 `x64-windows-static-md` triplet；只有启用动态 Loader 时才使用
`x64-windows`。`cli/ksm.py` 会分别使用 `.tmp/vcpkg-installed` 和
`.tmp/vcpkg-installed-webview2-loader`，避免两种模式互相清理依赖。

## 独立示例

示例工程位于 `windows/examples/win32-host/`，使用
`find_package(Kasumi CONFIG REQUIRED)`。默认构建会把共享目录
`examples/showcase/` 编译进 EXE，输出目录不需要 `web/`。WebView2 Loader 默认静态
链接，WebView2 用户数据写入 `%LOCALAPPDATA%\Kasumi\Example\WebView2`，不会在 EXE
旁生成 `.WebView2` 目录。消费者只获得静态库、公共头文件和导入目标，不获得框架
`.cpp` 源码。

```powershell
pnpm ksm dev
pnpm ksm build --windows --target example
pnpm ksm build --windows --target example --with-webview2-loader
python cli/ksm.py build --platform windows --target sdk
cmake -S windows/examples/win32-host `
  -B windows/examples/win32-host/build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="$PWD/windows/build/sdk/install" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build windows/examples/win32-host/build --config Release
.\windows\examples\win32-host\build\Release\kasumi_win32_example.exe
```

示例要求 Evergreen WebView2 Runtime。缺失时显示安装提示并退出，不内置 Fixed Version
Runtime。默认使用静态 WebView2 Loader，不生成 `WebView2Loader.dll`。只有显式传入
`--with-webview2-loader` 或设置 `KASUMI_BUNDLE_WEBVIEW2_LOADER=ON` 时，才会改用动态
Loader 并把 `WebView2Loader.dll` 复制到 EXE 同级。宿主应用数据写入
`%LOCALAPPDATA%\Kasumi\Example`，默认不会在项目目录创建 `KasumiData/` 或
`kasumi_win32_example.exe.WebView2`。

需要直接替换页面文件时，可以启用外置模式：

```powershell
cmake -S windows/examples/win32-host `
  -B windows/examples/win32-host/build-external `
  -DKASUMI_EXAMPLE_EXTERNAL_WEB=ON `
  -DCMAKE_PREFIX_PATH="$PWD/windows/build/sdk/install" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

外置模式会把资源复制为 EXE 同级的 `web/`。运行时参数 `--web-root <dir>` 和
`--user-data-folder <dir>` 可分别覆盖 Web 根目录和 WebView2 用户数据目录。
命令行构建与打包可传入 `--external-web` 选择该模式，默认始终生成内嵌资源产物。

## WebView2 适配

`KSMWebView2Host` 构造时接收窗口、文件系统、外部 origin 白名单、可选 Web 根目录、
可选用户数据目录和可选资源提供器。初始化时：

1. 检测 Evergreen WebView2 Runtime。
2. 创建 WebView2 环境、Controller 和 CoreWebView2。
3. `web_root` 为空时通过 `WebResourceRequested` 返回 EXE 内嵌资源；非空时把
   `https://app.local` 映射到该目录。
4. 关闭 DevTools、默认上下文菜单和状态栏。
5. 校验消息来源，拦截非 `app.local` 或开发 origin 的导航和 `window.open`。
6. 通过 `PostMessageAsJson` 回到 UI 线程发送响应。
7. 在 `WM_SIZE` 时发送物理内容区尺寸的 `window.resized` 事件。

`user_data_folder` 为空时使用
`%LOCALAPPDATA%\Kasumi\WebView2\<executable-name>`，不会让 WebView2 在 EXE 旁创建
默认 profile。内嵌模式为 `app://res` 和 `app://assets` 提供只读目录枚举与文件读取，
CSP 由 `index.html` 响应头注入。

每个窗口分配单调递增且不复用的 `windowId`。同一窗口复用同一个路由器和宿主服务实例，
关闭后从注册表移除并抑制迟到响应。

## 下载、链接与窗口

`KSMWindowsHostServices` 使用 WinHTTP 下载：

- 只接受 HTTPS，关闭自动重定向并逐跳校验。
- 最多 5 次重定向，总超时 120 秒，最大 100 MiB。
- 下载到同目录 `.part` 文件，成功后用 `MoveFileExW` 提交。
- 取消或超时会关闭 WinHTTP 句柄并删除临时文件。
- `overwrite` 缺省为 `false`；目标存在返回 `IO_ERROR`。

`shell.openExternal` 使用 `ShellExecuteW`，服务层再次执行精确 origin 白名单校验。
默认空白名单，调用方必须通过 `KSMWindowsHostServices` 或 `KSMWebView2Host` 构造参数
显式配置。

`window.setSize` 接受逻辑外框像素，最小 `320 × 240`。实现使用 `GetDpiForWindow`、
`MulDiv`、`MonitorFromWindow`、`GetMonitorInfo` 和 `SetWindowPos`；最大化、最小化和
全屏返回 `NOT_SUPPORTED`。`window.resized` 仍使用物理内容区尺寸。

## 测试

```powershell
python cli/ksm.py test --platform windows --target sdk
python cli/ksm.py test --platform windows --target example
```

| 测试 | 位置 | 覆盖内容 |
| --- | --- | --- |
| `kasumi_contract` | SDK 构建目录 | 路由、策略、文件、权限、超时、取消和根入口 |
| `kasumi_win32_example_smoke` | 示例构建目录 | 隐藏启动示例、加载页面、执行 `ping` 并退出 |

示例 smoke test 要求机器已安装 Evergreen WebView2 Runtime。

## 打包与签名

```powershell
python cli/ksm.py package --platform windows --target sdk
python cli/ksm.py package --platform windows --target example
pnpm ksm package --windows --target example --with-webview2-loader
```

- SDK 产物：`windows/build/sdk/packages/kasumi-windows-sdk-0.1.0.zip`。
- 示例产物：`windows/examples/win32-host/build/packages/kasumi-win32-example-0.1.0.zip`。
- SDK ZIP 只包含静态库、公共头文件、CMake 包配置和 README。
- 默认示例 ZIP 只包含 EXE 和内嵌 Web 资源，不包含 `WebView2Loader.dll`、`.WebView2`、`web/` 或框架源码。
- `--with-webview2-loader` 模式额外包含 `WebView2Loader.dll`；外置模式额外包含 `web/`。

标签发布要求：

| Secret | 用途 |
| --- | --- |
| `WINDOWS_CERTIFICATE_BASE64` | Authenticode PFX 的 Base64 |
| `WINDOWS_CERTIFICATE_PASSWORD` | PFX 口令 |

正式标签缺少证书时工作流直接失败。签名使用 SHA-256 和 RFC3161 时间戳，并对每个
EXE/DLL 执行 `signtool verify /pa /all`。普通 CI 可以构建未签名产物。

## 排查路径

1. 启动提示缺少 WebView2 Runtime：安装 Evergreen Runtime 后重新运行。
2. `find_package(Kasumi)` 失败：检查 `CMAKE_PREFIX_PATH` 是否指向 SDK 安装目录。
3. 链接失败：确认消费者链接的是 `Kasumi::kasumi_windows_bridge`。
4. 页面空白：默认模式检查内嵌资源是否重新生成；外置模式检查 `web/index.html` 与 EXE 同级。
5. 请求一直超时：检查 WebView2 消息是否回到 UI 线程，来源是否为 `https://app.local`。
6. 外部链接被拒绝：检查 origin 是否包含正确 scheme、host 和端口。
7. 下载失败：检查 HTTPS、重定向、目标路径、覆盖策略和 `.part` 清理。
8. 窗口尺寸不变：检查窗口是否最大化、最小化或全屏，以及目标显示器工作区。
