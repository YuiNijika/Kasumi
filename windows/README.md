# Windows host

> 提供 Windows 宿主侧的 KSM 命令实现和桥接入口。

## 当前桥接入口

| 文件 | 责任 |
| --- | --- |
| `src/commands/ksm_phase1.h` | `ping` 与 `capabilities.query` 的结果模型和实现入口 |
| `src/bridge/ksm_command_registry.h` | Windows 已实现命令清单 |
| `src/bridge/ksm_router.h` | JSON 请求校验、命令分发和标准响应 |
| `src/generated/contract.h` | 生成的命令、权限、事件和参数校验器 |
| `src/platform/ksm_host_services.h` | 宿主平台服务接口 |
| `src/platform/ksm_windows_host_services.h` | 基于 Win32 的对话框、文件、剪贴板、通知和窗口实现 |

`KSMRouter::HandleMessage` 接收 WebView2 消息正文，并通过回调返回响应正文。`KSMWebViewHost` 持有单个路由器，因此事件订阅状态可以跨消息保留；构造时传入 `KSMWindowsHostServices` 即可启用平台命令。

`KSMWebViewHost` 使用固定工作线程池执行命令，最多同时执行 8 条；超过上限返回 `BUSY`，排队中的请求可以用 `cancel` 消息取消。响应回调必须由实际 WebView2 适配器切回创建 WebView2 的 UI 线程。

文件选择器返回 `app://picked/<id>`，真实路径只保存在 `KSMWindowsHostServices` 内部。单文件读写限制为 16MB。

`KSMWebViewHost::NotifyWebViewReloaded` 和 `KSMWebViewHost::NotifyWindowResized` 用于把宿主事件发送给已订阅的页面。实际 WebView2 宿主还必须在导航、窗口创建和消息来源处调用 `IsAllowedOrigin`，并设置 CSP。
