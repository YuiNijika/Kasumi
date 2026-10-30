# Windows host

> 说明 Windows 侧的 KSM 命令实现、WebView2 适配、可运行示例、打包和发布签名。

## 模块边界

| 文件                                       | 责任                                                      |
| ------------------------------------------ | --------------------------------------------------------- |
| `src/commands/ksm_phase1.h`                | 宿主侧 `KSM` 根入口、生命周期和统一调用                   |
| `src/bridge/ksm_command_registry.h`        | Windows 已实现命令清单                                    |
| `src/bridge/ksm_router.h`                  | 参数校验、权限检查和命令分发                              |
| `src/platform/ksm_host_services.h`         | 宿主平台服务接口                                          |
| `src/platform/ksm_windows_host_services.h` | Win32 文件、对话框、剪贴板、通知、下载、链接和窗口服务    |
| `src/platform/ksm_webview_host.h`          | 工作线程、超时、取消、单响应和事件投递                    |
| `src/platform/ksm_webview2_host.h`         | WebView2 创建、消息来源、导航拦截、窗口事件和 UI 线程回投 |
| `examples/win32-host/`                     | 可运行的 Win32 + WebView2 示例                            |
| `tests/contract_runner.cpp`                | Windows 契约、安全策略和生命周期测试                      |

Windows 当前实现 **28/28 个适用命令**。`device.getInfo` 是 Android 专属命令，Windows
返回 `NOT_SUPPORTED`。

## 宿主入口

```cpp
KSM::Init(file_system, host_services);
const std::string response = KSM::Invoke(
    "window-1",
    "http.download",
    R"({"url":"https://example.com/file.bin","path":"app://data/file.bin"})"
);
```

`KSM::Invoke(windowId, ...)` 按窗口维护独立路由器和事件订阅。空 `windowId` 使用默认
路由上下文。宿主根入口直接调用 `KSMHostServices`，不得通过 `KSM::Invoke` 递归回路由。

| 子模块   | 入口                | 宿主服务                           |
| -------- | ------------------- | ---------------------------------- |
| 文件系统 | `KSM::Fs`           | `KSMFileSystem` 与 picked 文件方法 |
| 对话框   | `KSM::Dialog`       | 对话框与文件选择方法               |
| 剪贴板   | `KSM::Clipboard`    | 读写系统剪贴板                     |
| 通知     | `KSM::Notification` | `ShowNotification`                 |
| 权限     | `KSM::Permission`   | 生成的权限清单                     |
| 窗口     | `KSM::Window`       | 标题、最小化、最大化、尺寸和关闭   |
| HTTP     | `KSM::Http`         | `Download`                         |
| Shell    | `KSM::Shell`        | `OpenExternal`                     |
| 设备     | `KSM::Device`       | `GetInfo`，Windows 返回空          |
| 应用     | `KSM::App`          | 版本、语言和退出                   |

## WebView2 适配

`KSMWebView2Host` 构造时接收窗口、文件系统、外部 origin 白名单、Web 根目录和独立用户数据目录。
初始化时：

1. 检测 Evergreen WebView2 Runtime。
2. 创建 WebView2 环境、Controller 和 CoreWebView2。
3. 将 `https://app.local` 映射到 Web 根目录。
4. 关闭 DevTools、默认上下文菜单和状态栏。
5. 校验消息来源，拦截非 `app.local` / 开发 origin 导航和 `window.open`。
6. 通过 `PostMessageAsJson` 回到 UI 线程发送响应。
7. 在 `WM_SIZE` 时发送物理内容区尺寸的 `window.resized` 事件。

每个窗口分配单调递增且不复用的 `windowId`。同一窗口复用同一个路由器和宿主服务实例，
关闭后从注册表移除并抑制迟到响应。

## 下载、链接与窗口

`KSMWindowsHostServices` 使用 WinHTTP 下载：

- 只接受 HTTPS，关闭自动重定向并逐跳校验。
- 最多 5 次重定向，总超时 120 秒，最大 100 MiB。
- 下载到同目录 `.part` 文件，成功后用 `MoveFileExW` 提交。
- 取消或超时会关闭 WinHTTP 句柄并删除临时文件。
- `overwrite` 缺省为 `false`；目标存在返回 `IO_ERROR`。

`shell.openExternal` 使用 `ShellExecuteW`，服务层再次执行精确 origin 白名单校验。默认空白名单，
调用方必须通过 `KSMWindowsHostServices` 或 `KSMWebView2Host` 构造参数显式配置。

`window.setSize` 接受逻辑外框像素，最小 `320 × 240`。实现使用 `GetDpiForWindow`、`MulDiv`、
`MonitorFromWindow`、`GetMonitorInfo` 和 `SetWindowPos`；最大化、最小化和全屏返回
`NOT_SUPPORTED`。`window.resized` 仍使用物理内容区尺寸。

## 可运行示例

`examples/win32-host/` 构建出 `kasumi_win32_example.exe`，加载
`https://app.local/index.html`，展示 Ping、文件、窗口尺寸、外部链接和下载。

```powershell
$env:CMAKE_TOOLCHAIN_FILE = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake -S windows -B windows/build -DCMAKE_TOOLCHAIN_FILE="$env:CMAKE_TOOLCHAIN_FILE"
cmake --build windows/build --config Release
.\windows\build\Release\kasumi_win32_example.exe
```

示例要求 Evergreen WebView2 Runtime。缺失时显示安装提示并退出，不内置 Fixed Version Runtime。
`web/` 必须与 EXE 保持同级；示例数据写入 `%LOCALAPPDATA%\Kasumi\Example`。

CTest 包含：

- `kasumi_contract`：路由、策略、文件、权限、超时、取消和根入口测试。
- `kasumi_win32_example_smoke`：隐藏启动示例，页面加载后执行 `ping` 并通过 `app.exit` 退出。

## 打包与签名

```bash
python cli/ksm.py package --platform windows
```

CPack 生成 `windows/build/packages/kasumi-windows-bridge-0.1.0.zip`，包含静态库、公共头文件、
示例 EXE、Web 资源和 `EXAMPLE_HOST.md`。`vcpkg.json` 固定 `builtin-baseline`，依赖
`nlohmann-json` 和 `webview2`。

标签发布要求：

| Secret                         | 用途                       |
| ------------------------------ | -------------------------- |
| `WINDOWS_CERTIFICATE_BASE64`   | Authenticode PFX 的 Base64 |
| `WINDOWS_CERTIFICATE_PASSWORD` | PFX 口令                   |

正式标签缺少证书时工作流直接失败。签名使用 SHA-256 和 RFC3161 时间戳，并对每个 EXE/DLL
执行 `signtool verify /pa /all`。普通 CI 可以构建未签名产物。

## 排查路径

1. 启动提示缺少 WebView2 Runtime：安装 Evergreen Runtime 后重新运行。
2. 页面空白：确认 `web/index.html` 与 EXE 同级，虚拟主机映射成功。
3. 请求一直超时：检查 WebView2 消息是否回到 UI 线程，来源是否为 `https://app.local`。
4. 外部链接被拒绝：检查 origin 是否包含正确 scheme、host 和端口。
5. 下载失败：检查 HTTPS、重定向、目标路径、覆盖策略和 `.part` 清理。
6. 窗口尺寸不变：检查窗口是否最大化、最小化或全屏，以及目标显示器工作区。
7. 发布失败：检查证书 secrets、`signtool` 路径和 ZIP 必需文件。
