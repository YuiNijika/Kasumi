# Android host

> 提供 Android 宿主侧的 KSM 命令实现和桥接入口。

## 当前桥接入口

| 文件 | 责任 |
| --- | --- |
| `app/src/main/java/dev/kasumi/app/bridge/KSMPhase1.kt` | `ping` 与 `capabilities.query` 的结果模型和实现入口 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMCommandRegistry.kt` | Android 已实现命令清单 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMRouter.kt` | JSON 请求校验、命令分发和标准响应 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMHostServices.kt` | Android 平台服务接口 |
| `app/src/main/java/dev/kasumi/app/KSMAndroidHostServices.kt` | 对话框、文件选择、剪贴板、通知和窗口实现 |
| `app/src/main/assets/index.html` | 带 CSP 的最小宿主页面 |
| `app/src/test/java/.../ContractTest.kt` | 命令契约测试 |

`KSMRouter.handleMessage` 接收 WebView 消息正文并返回响应正文。`WebViewCompat.addWebMessageListener` 适配器将命令放到 IO 协程执行，最多同时执行 8 条；超过上限返回 `BUSY`，取消消息会中断当前等待并返回 `CANCELLED`。

前端 `KSMWebMessageTransport` 会自动识别 Windows 的 `window.chrome.webview` 和 Android 注入的 `window.ksm`。

Android 13 及以上的通知需要先调用 `permission.request` 请求 `notification.show`，应用清单已声明 `POST_NOTIFICATIONS`。

WebView 只允许 `https://app.local` 和开发用 `http://localhost:5173`。其他导航交给系统浏览器，`window.open` 被拒绝，生产构建关闭 WebView 调试。文件选择器返回 `app://picked/<id>`，真实 `content://` URI 不离开宿主层。
