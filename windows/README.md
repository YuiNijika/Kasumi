# Windows host

> 提供 Windows 宿主侧的 KSM 命令实现和桥接入口。

## 当前桥接入口

| 文件 | 责任 |
| --- | --- |
| `src/commands/ksm_phase1.h` | 宿主侧 `KSM` 根入口、`ping` 与能力查询模型 |
| `src/bridge/ksm_command_registry.h` | Windows 已实现命令清单 |
| `src/bridge/ksm_router.h` | JSON 请求校验、命令分发和标准响应 |
| `src/generated/contract.h` | 生成的命令、权限、事件和参数校验器 |
| `src/platform/ksm_host_services.h` | 宿主平台服务接口 |
| `src/platform/ksm_windows_host_services.h` | 基于 Win32 的对话框、文件、剪贴板、通知和窗口实现 |

`KSMRouter::HandleMessage` 接收 WebView2 消息正文，并通过回调返回响应正文。`KSMWebViewHost` 持有单个路由器，因此事件订阅状态可以跨消息保留；构造时传入 `KSMWindowsHostServices` 即可启用平台命令。

宿主侧根入口使用 `KSM::Init` 注入文件系统与平台服务，之后通过 `KSM::Invoke`、
`KSM::On`、`KSM::Get` 和各子模块访问能力。`KSM::Invoke` 返回标准响应 JSON，错误不会伪装成
成功结果。原生事件监听通过 `KSM::On` 注册，并由宿主在事件到达时调用 `KSM::EmitEvent` 分发。
`KSM::Invoke(windowId, ...)` 按窗口维护独立路由器，事件订阅和权限状态不会跨窗口复用；空
`windowId` 使用独立的默认路由上下文。

| 子模块 | 入口 | 宿主服务 |
| --- | --- | --- |
| 文件系统 | `KSM::Fs` | `KSMFileSystem` 与 `ReadPickedText` / `WritePickedText` / `PickedExists` / `PickedStat` / `RemovePicked` |
| 对话框 | `KSM::Dialog` | `KSMHostServices` 对话框与文件选择方法 |
| 剪贴板 | `KSM::Clipboard` | `ReadClipboardText` / `WriteClipboardText` |
| 通知 | `KSM::Notification` | `ShowNotification` |
| 权限 | `KSM::Permission` | 校验生成的权限清单，Windows 直接授予已注册权限 |
| 窗口 | `KSM::Window` | 标题、最小化、最大化和关闭方法 |
| 应用 | `KSM::App` | 版本、语言和退出方法 |

`KSM::Permission::Request` 对未知权限返回 `std::nullopt`，未初始化时同样不可用。根入口子模块
用于可信原生代码，前端命令仍必须经过 `KSMRouter` 的参数、权限和路径校验。

`KSMWebViewHost` 使用固定工作线程池执行命令，最多同时执行 8 条；超过上限返回 `BUSY`，排队中的请求可以用 `cancel` 消息取消。响应回调必须由实际 WebView2 适配器切回创建 WebView2 的 UI 线程。

文件选择器返回 `app://picked/<id>`，真实路径只保存在 `KSMWindowsHostServices` 内部。
`app://data`、`app://cache`、`app://tmp` 可读写，`app://res` 与 `app://assets`
只读。`app://picked/<id>` 通过宿主服务读写。单文件读写限制为 16MB。
未注册的虚拟根、盘符路径、NTFS 流、反斜杠和越权 `app://picked` 路径都会在原生路由层被拒绝。

目录与元数据方法使用同一套虚拟路径解析。`KSM::Fs::ReadDir` 支持可写沙箱和只读资源，
`MakeDirectory` 只允许 `data`、`cache`、`tmp`，`Stat` 还支持有效 picked 句柄。
目录项按名称排序，`modifiedAt` 为 Unix 毫秒或 `null`，目录 `size` 为 `0`；符号链接不会被跟随。

`KSMWebViewHost::NotifyWebViewReloaded` 发送系统事件 `webview.reloaded`，不依赖页面订阅；
`KSMWebViewHost::NotifyWindowResized` 发送可订阅的 `window.resized`。实际 WebView2 宿主还必须
在导航、窗口创建和消息来源处调用 `IsAllowedOrigin` / `IsAllowedNavigation`，并设置 CSP。
来源校验接受 `https://app.local`、`http://localhost:5173` 以及它们的完整文档 URL，拒绝端口
变体、近似域名和非 HTTP scheme。

原生超时与取消由 `KSMWebViewHost` 处理，`RespondOnce` 保证单个请求只返回一次。Windows
阻塞式对话框无法真正被线程终止，超时或取消后只抑制迟到响应。宿主销毁时会把未完成请求
标记为结束并停止投递迟到响应。

契约测试入口是 `tests/contract_runner.cpp`，由 CMake 注册为 `kasumi_contract`。测试覆盖根入口
全部子模块、目录读取与创建、文件元数据、picked 文件、未知权限拒绝、消息来源校验、超时取消、
销毁后迟到响应抑制，以及 `KSM::Shutdown` 后所有子模块不可用。

## 打包

CMake 安装目标会输出静态库和 `src/` 下的公共头文件。执行：

```bash
python cli/ksm.py package --platform windows
```

依赖由 vcpkg 管理时，先设置 `CMAKE_TOOLCHAIN_FILE`；CLI 会把该变量传给 CMake。

```powershell
$env:CMAKE_TOOLCHAIN_FILE = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
python cli/ksm.py package --platform windows
```

CPack 生成 `windows/build/packages/kasumi-windows-bridge-0.1.0.zip`。当前产物是框架 SDK
压缩包，不包含可执行宿主程序；Windows 可执行文件的 Authenticode 签名要等实际应用壳接入后
再配置。

Windows CMake 目标会定义 `UNICODE` / `_UNICODE`。`vcpkg.json` 固定
`builtin-baseline`；升级依赖时应同步更新 baseline，避免新版 vcpkg 拒绝清单。
