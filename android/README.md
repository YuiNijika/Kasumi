# Android host

> 提供 Android 宿主侧的 KSM 命令实现和桥接入口。

## 当前桥接入口

| 文件 | 责任 |
| --- | --- |
| `app/src/main/java/dev/kasumi/app/bridge/KSM.kt` | 宿主侧根入口、生命周期、统一调用和原生事件监听 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMFs.kt` | 文件系统子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMDialog.kt` | 对话框子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMClipboard.kt` | 剪贴板子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMNotification.kt` | 通知子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMPermission.kt` | 权限子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMWindow.kt` | Android 窗口关闭子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMApp.kt` | 应用信息与退出子模块 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMPhase1.kt` | `ping`、能力和应用信息数据模型 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMCommandRegistry.kt` | Android 已实现命令清单 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMRouter.kt` | JSON 请求校验、命令分发和标准响应 |
| `app/src/main/java/dev/kasumi/app/bridge/KSMHostServices.kt` | Android 平台服务接口 |
| `app/src/main/java/dev/kasumi/app/KSMAndroidHostServices.kt` | 对话框、文件选择、剪贴板、通知和窗口实现 |
| `app/src/main/assets/web/index.html` | 带 CSP 的最小宿主页面 |
| `app/src/test/java/.../ContractTest.kt` | 命令契约测试 |

`KSMRouter.handleMessage` 接收 WebView 消息正文并返回响应正文。`WebViewCompat.addWebMessageListener` 适配器将命令放到 IO 协程执行，最多同时执行 8 条；超过上限返回 `BUSY`，取消消息会中断当前等待并返回 `CANCELLED`。

`MainActivity` 创建路由器后调用 `KSM.init` 注入同一个路由器和文件系统。宿主侧通过
`KSM.invoke` 调用命令，通过 `KSM.get` 读取缓存能力，通过 `KSM.fs`、`KSM.dialog`、
`KSM.clipboard`、`KSM.notification`、`KSM.permission`、`KSM.window` 和 `KSM.app` 访问直接
能力；`KSM.on` 注册的原生监听由 `KSM.emitEvent` 分发。

`KSM.fs` 与协议路由保持一致：`app://data`、`app://cache`、`app://tmp` 使用
`KSMFileSystem`，`app://res` 和 `app://assets` 通过资源服务读取，`app://picked/<id>` 通过宿主
保存的 `content://` 映射读写。直接子模块仍不向调用方暴露真实路径。

`KSM.fs.readDir` 支持可写沙箱和只读资源，`KSM.fs.makeDirectory` 只允许可写沙箱，
`KSM.fs.stat` 还支持有效 picked 句柄。目录项按名称排序，`modifiedAt` 为 Unix 毫秒或
`null`，目录 `size` 为 `0`；符号链接不会被跟随。

`KSM.permission.request` 只接受生成契约中的权限名，未知权限抛出 `IllegalArgumentException`。
`KSM.notification.show` 会先检查 `notification.show` 的运行时权限。Android 协议只支持
`window.close`，因此根入口的 `KSM.window` 不提供桌面端标题、最小化和最大化方法。
应用信息中的版本与语言来自 `KSMAndroidHostServices`，不再由根入口写死。

前端 `KSMWebMessageTransport` 会自动识别 Windows 的 `window.chrome.webview` 和 Android 注入的 `window.ksm`。
命令响应走注入对象，Android 的系统和路由事件走同窗口 `window.postMessage`；传输层同时监听
两条通路，并拒绝来源不是当前窗口的 `message` 事件。

Android 13 及以上的通知需要先调用 `permission.request` 请求 `notification.show`，应用清单已声明 `POST_NOTIFICATIONS`。

WebView 只允许 `https://app.local` 和开发用 `http://localhost:5173`。其他 `http` / `https`
导航交给系统浏览器，`intent:`、`file:` 等 scheme 会被直接拒绝；`window.open` 被拒绝，生产
构建关闭 WebView 调试。文件选择器返回 `app://picked/<id>`，真实 `content://` URI 不离开宿主层。

`app://data`、`app://cache`、`app://tmp` 可读写；`app://res` 映射到 APK 的 `assets/res/`，
`app://assets` 映射到 `assets/web/`，两者只读。超过 64KiB 的读取结果写入
`app://tmp/read-<id>.txt`，再次读取该句柄时返回同一路径。
未注册的虚拟根、盘符路径、NTFS 流、反斜杠和越权 `app://picked` 路径都会在原生路由层被拒绝。

命令在 IO 协程中执行，最多 8 条并发；超过上限返回 `BUSY`，超时返回 `TIMEOUT`，
取消返回 `CANCELLED`。`webview.reloaded` 由 `onPageFinished` 作为系统事件发送，
`window.resized` 由布局变化发送并需要订阅。

`ContractTest` 覆盖根入口全部子模块、目录读取与创建、文件元数据、只读与 picked 路径、
未知权限拒绝，以及 `KSM.shutdown()` 后子模块不可用；`NavigationPolicyTest` 验证 WebView
来源只接受配置的 origin，并限制外部导航为 `http` / `https`。
仍需在真机上验证通知运行时权限、剪贴板、文件选择器和窗口关闭行为。

## 发布签名

release 构建从以下环境变量读取签名材料：

| 环境变量 | 用途 |
| --- | --- |
| `KASUMI_ANDROID_KEYSTORE` | keystore 文件绝对路径 |
| `KASUMI_ANDROID_KEYSTORE_PASSWORD` | keystore 口令 |
| `KASUMI_ANDROID_KEY_ALIAS` | key alias |
| `KASUMI_ANDROID_KEY_PASSWORD` | key 口令 |

四个变量都有效且 keystore 文件存在时，`release` 自动使用该签名配置；否则构建保持未签名，
便于本地验证。发布工作流还会使用 `apksigner verify --print-certs` 校验签名。

```bash
python cli/ksm.py package --platform android
```
