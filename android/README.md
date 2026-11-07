# Android SDK

> 说明 Android 侧的 AAR、宿主服务、WebView 适配、生命周期、测试、独立示例和发布签名。

## 模块边界

`android/kasumi/` 是发布给应用消费者的 Android Library 模块。`android/app/` 只作为
框架维护者的测试宿主，负责 JVM 与 instrumentation 测试，不是消费者模板。

| 路径 | 责任 |
| --- | --- |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSM.kt` | 根入口、生命周期、统一调用和原生事件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMRouter.kt` | JSON 校验、权限检查和命令分发 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMHostServices.kt` | Android 平台服务接口 |
| `kasumi/src/main/java/dev/kasumi/app/KSMAndroidHostServices.kt` | 对话框、文件、剪贴板、通知、下载、链接、设备、系统信息和日志 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMPlugin.kt` | 插件接口、注册表和默认插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMClipboardPlugin.kt` | 剪贴板命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMNotificationPlugin.kt` | 通知命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMWindowPlugin.kt` | 关闭窗口插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMHttpPlugin.kt` | HTTPS 下载命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMShellPlugin.kt` | 外部链接命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMDevicePlugin.kt` | 非持久设备信息插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMSqlDriver.kt` | SQL 驱动 SPI 和内置 SQLite 实现 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMSqlPlugin.kt` | SQL 命令插件与宿主驱动分发 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMPathPlugin.kt` | 虚拟路径命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMStorePlugin.kt` | 应用 JSON 存储命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMOperatingSystemPlugin.kt` | 系统信息命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMLogPlugin.kt` | 平台日志命令插件 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMWebViewHost.kt` | WebView 消息桥、命令并发、导航策略、生命周期和资源释放 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMWebViewRuntime.kt` | WebView 默认设置、全屏窗口、前后台暂停和低内存回收 |
| `kasumi/src/main/res/values/styles.xml` | 无 ActionBar 的 `Theme.Kasumi.Fullscreen` Activity 主题 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMHttpPolicy.kt` | URL、origin 和重定向策略 |
| `kasumi/src/main/AndroidManifest.xml` | 网络与通知权限声明 |
| `app/src/main/java/dev/kasumi/app/MainActivity.kt` | 内部测试宿主的 WebView 与生命周期装配 |
| `app/src/test/` | JVM 契约与策略测试 |
| `app/src/androidTest/` | instrumentation 与真实 Activity 测试 |

Android 当前实现 **44/44 个适用命令**。Windows 专属的窗口标题、最小化、最大化、
尺寸调整不提供；`device.getInfo` 仅 Android 提供。

## 应用配置

Android 示例从仓库根目录 `kasumi.config.json` 读取 `android.packageName`、
`android.version`、`android.commands`、`android.permissions`，以及可选的
`android.shell.allowedOrigins`、`android.http.allowedOrigins` 和
`android.webview`：

```json
{
  "android": {
    "packageName": "dev.kasumi.example",
    "version": "0.1.0",
    "commands": [
      "ping",
      "capabilities.query",
      "app.getInfo",
      "http.download"
    ],
    "permissions": ["http.download"],
    "shell": {
      "allowedOrigins": ["https://example.com"]
    },
    "http": {
      "allowedOrigins": ["https://cdn.example.com"]
    },
    "webview": {
      "productionEntry": "https://app.local/index.html",
      "developmentEntries": [
        "http://localhost:5173/index.html",
        "http://localhost:5173/",
        "http://localhost:5173"
      ],
      "supportedHtmlFiles": ["index.html"]
    }
  }
}
```

`packageName` 注入 Gradle `applicationId`，同时用于 `pnpm ksm dev --android` 的
启动、停止和卸载目标；`version` 注入 `versionName`，示例再把
`BuildConfig.VERSION_NAME` 传给 `KSMAndroidHostServices`，因此 `app.getInfo.version`
与配置一致。`commands` 生成 `BuildConfig.KSM_COMMANDS` 并传入 `KSMRouter`，
`permissions` 生成 `BuildConfig.KSM_PERMISSIONS` 并传入路由器，缺失或空数组表示拒绝全部
权限。`ping` 与 `capabilities.query` 必须保留；未列入的命令返回 `NOT_SUPPORTED`，命令已
启用但权限未配置时返回 `PERMISSION_DENIED`，Android 原生 `KSM` 子模块同样不能绕过配置。
`permission.request` 不会动态扩大静态集合。Gradle `namespace` 仍固定为源码包
`dev.kasumi.example`，避免仅修改应用 ID 时破坏 Kotlin 源码目录。

`shell.allowedOrigins` 接受精确 `http` / `https` origin；`http.allowedOrigins` 只接受
精确 `https` origin。两者默认拒绝，不支持通配符或用户名密码，每一跳下载重定向都要重新匹配。

| `android.webview` 字段 | 作用 |
| --- | --- |
| `productionEntry` | 生产构建加载的 `https://app.local/*.html` 入口 |
| `developmentEntries` | debug 构建允许 CLI 通过 `ksmDevUrl` 传入的完整 URL，覆盖 `/`、无路径与 HTML 文件形式 |
| `supportedHtmlFiles` | WebView 导航与 `KSMWebAssetLoader` 允许加载的 HTML 资源白名单 |

三项均可省略，默认值与上例相同。生产入口与开发 HTML 入口必须落在 `supportedHtmlFiles`
中；开发入口必须包含生产入口使用的 HTML 文件，CLI 也会据此选择 Vite、Next.js 与 Nuxt
的启动 URL。URL 不接受查询、片段、凭据、编码路径或父目录段。配置经 Node / Python CLI
写入 Gradle 的 `KSM_WEBVIEW_*` BuildConfig，再传入 `KSMWebViewConfig`；直接调用 SDK 时
可以显式构造同一个配置对象。

CLI 默认读取 `kasumi.config.json`，也可用 `--config <path>` 指定其他文件。直接运行
Gradle 时可用 `-PksmPackageName`、`-PksmVersion` 和逗号分隔的 `-PksmCommands`
覆盖默认值；入口策略对应 `-PksmWebViewProductionEntry`、
`-PksmWebViewDevelopmentEntries` 与 `-PksmWebViewSupportedHtmlFiles`。

## SDK 构建

```bash
pnpm ksm build --android --target sdk
python cli/ksm.py build --platform android --target sdk
```

该命令执行：

```bash
cd android
./gradlew :kasumi:assembleRelease
./gradlew :kasumi:publishReleasePublicationToLocalRepository
```

AAR 位于 `android/kasumi/build/outputs/aar/kasumi-release.aar`，本地 Maven 仓库位于
`android/kasumi/build/repo`。消费者使用：

```kotlin
dependencies {
    val kasumiVersion = rootProject.extra["kasumiVersion"] as String
    implementation("dev.kasumi:kasumi:$kasumiVersion")
}
```

发布变体只输出 AAR、POM 和 Gradle Module Metadata，不生成 sources JAR，也不把
`android/kasumi/src/**/*.kt` 复制给消费者。

Windows上如果 `TEMP`包含 `ADMINI~1`一类短文件名，先设置
`JAVA_TOOL_OPTIONS=-Djdk.net.unixdomain.tmpdir=<workspace>/.tmp/afunix`
再运行 Wrapper，否则 JDK21的 wepoll初始化会报告 loopback连接失败。

独立示例工程位于 `examples/android-host/`，只消费 AAR，不包含框架 Kotlin 源码：

```bash
pnpm ksm build --android --target example
pnpm ksm dev --android
python cli/ksm.py build --platform android --target sdk
cd examples/android-host
./gradlew assembleDebug
```

## 宿主入口

```kotlin
import dev.kasumi.app.KSMAndroidHostServices
import dev.kasumi.app.bridge.KSM
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMWebViewHost

val fileSystem = KSMFileSystem(dataDir, cacheDir, tmpDir)
val hostServices = KSMAndroidHostServices(
    activity = this,
    allowedExternalOrigins = setOf("https://example.com"),
    fileSystem = fileSystem
)
val webViewHost = KSMWebViewHost(
    activity = this,
    fileSystem = fileSystem,
    hostServices = hostServices,
    entryUrl = "https://app.local/index.html",
    fullscreen = true
)
setContentView(webViewHost.webView)
webViewHost.start()

val response = KSM.invoke(
    "window-1",
    "http.download",
    JSONObject()
        .put("url", "https://example.com/file.bin")
        .put("path", "app://data/file.bin")
)
```

`KSM` 根入口挂载 `fs`、`dialog`、`clipboard`、`notification`、`permission`、`http`、
`shell`、`device`、`sql`、`path`、`store`、`os`、`log`、`window` 和 `app`。直接子模块
用于可信原生代码；前端命令仍
必须经过 `KSMRouter` 的参数、权限和路径校验。

## 插件与 SQL

Android 默认插件由 `ksmDefaultPlugins()` 创建，包括 `fs`、`dialog`、`clipboard`、
`notification`、`window`、`http`、`shell`、`device`、`sql`、`path`、`store`、`os` 和
`log`。`core` 由路由层实现。
自定义插件实现 `KSMPlugin` 并通过 `KSMWebViewHost` 的 `plugins` 参数注册：

```kotlin
val plugin = object : KSMPlugin {
    override val id = "com.example.echo"
    override val commands = setOf("com.example.echo.ping")
    override val permissions = setOf("com.example.echo")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("com.example.echo")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The plugin was not authorized."
            )
        }
        return JSONObject().put("pong", true)
    }
}

val webViewHost = KSMWebViewHost(
    activity = this,
    fileSystem = fileSystem,
    hostServices = hostServices,
    entryUrl = "https://app.local/index.html",
    plugins = listOf(plugin)
)
```

除 `core` 外的内置插件都可以用同 ID 插件替换，但必须完整声明该插件的全部生成命令。
插件不得抢占其他生成命令。插件命令不在 `protocol/commands.json` 时，`validateArgs`
必须自己拒绝非法参数。插件上下文携带 `requestId`，下载插件用它连接宿主取消和超时链路。

SQL 默认注册 `KSMSqliteDriver`。需要 MySQL、PostgreSQL 或 MongoDB 时，实现
`KSMSqlDriver` / `KSMSqlConnection`，然后调用：

```kotlin
hostServices.registerSqlDriver(MyRemoteDriver())
```

只有注册成功的驱动才会出现在 `sql.drivers`。SQLite 接受 `:memory:` 或
`app://data`、`app://cache`、`app://tmp` 下的可写路径，不支持真实路径和
`app://picked`。远程驱动负责自己的连接串、TLS、占位符和游标映射，错误不得泄漏凭据。

每个宿主服务最多同时打开 32 个连接。单次查询最多返回 10,000 行和 16 MiB 结果数据，
超限返回 `RESOURCE_LIMIT`，不会截断结果。Store 插件使用进程级共享锁包裹完整读改写流程，
多个 WebView 的插件实例不会互相覆盖 `store.json`。

SQL 活动请求 ID 必须唯一，重复 ID 返回 `BUSY`。SQLite 查询使用 `CancellationSignal`；
取消或超时后活动查询被中断并返回 `CANCELLED` 或 `TIMEOUT`。

`KSMWebViewHost` 封装 `WebViewCompat.addWebMessageListener`，接收消息后放入专用
`ThreadPoolExecutor`，最多同时执行 8 条命令，排队上限为 32。超过上限返回 `BUSY`，超时
返回 `TIMEOUT`，取消消息只终止当前 Job 和活动连接并返回 `CANCELLED`，不会 interrupt
共享 `Dispatchers.IO` 线程。每个活动请求只保留一个可取消句柄，请求结束后立即移除，不维护
无界取消 ID 集合。`timeoutMs` 必须是 `1..600000` 范围内的整数。

每个 `KSMWebViewHost` 生成稳定且不复用的 `windowId`，并注册到 `KSM` 的窗口表。
`KSM.invoke(windowId, ...)` 和 `KSM.on(windowId, ...)` 只访问目标窗口；无窗口参数 API
仅在恰好注册一个窗口时可用。`onPageStarted` 会静默取消当前窗口旧请求并抑制迟到响应。
`close()` 只注销当前窗口，不会关闭共享的 `KSMAndroidHostServices`，服务生命周期由
Activity 或 Application 所有者统一管理。

Activity 必须把 `onResume`、`onPause`、`onTrimMemory`、`onLowMemory` 和 `onDestroy`
转发给 `KSMWebViewHost`，并在窗口焦点恢复时转发 `onWindowFocusChanged`。宿主在前后台
切换时暂停或恢复 WebView 定时器，在低内存时只清理 WebView 内存缓存并调用 `freeMemory`，
不会删除磁盘缓存；销毁时移除消息监听、布局监听、协程和宿主服务引用。

## 文件与事件

`app://data`、`app://cache`、`app://tmp` 可读写；`app://res` 映射到 APK 的
`assets/res/`，`app://assets` 映射到 `assets/web/`，两者只读。`app://picked/<id>`
通过宿主保存的 `content://` 映射读写，真实 URI 不离开宿主层。

所有路径操作都对可写根和目标执行 canonical containment 校验，并禁止叶节点跟随符号链接。
父目录穿越、符号链接逃逸和越界目录枚举返回 `PERMISSION_DENIED`；合法 picked 授权不受影响。

`fs.readDir` 返回按名称排序的目录项；`fs.mkdir` 只允许可写沙箱；`fs.stat` 支持可写
沙箱、只读资源和有效 picked 句柄。目录 `size` 为 `0`，`modifiedAt` 为 Unix 毫秒或
`null`。超过 64 KiB 的读取结果写入 `app://tmp/read-<id>.txt`，再次读取同一句柄返回
同一路径。

`webview.navigated` 在成功导航后发送；`webview.reloaded` 只在 `doUpdateVisitedHistory`
标记为 reload 时发送。`window.resized` 由布局变化发送，需要订阅，尺寸单位是物理内容区像素。

## HTTP、Shell 与 Device

### `http.download`

```kotlin
val result = KSM.http.download(
    url = "https://example.com/file.bin",
    path = "app://data/file.bin",
    overwrite = false
)
```

`KSMAndroidHostServices.download` 使用 `HttpsURLConnection`：

- 只接受 HTTPS，关闭自动重定向并逐跳校验。
- 每一跳都必须匹配 `android.http.allowedOrigins` 的精确 origin，并拒绝私网、回环、
  链路本地和云元数据地址。
- 最多 5 次重定向，总超时 120 秒，最大 100 MiB。
- 目标必须是 `app://data`、`app://cache` 或 `app://tmp` 下的文件。
- 目标已存在且未允许覆盖时返回 `IO_ERROR`。
- 下载到同目录临时 `.part` 文件，成功后提交；失败、取消或异常时删除。
- 请求 ID 在创建 `.part` 文件前原子占用；同一 ID 的并发下载返回 `BUSY`。
- 取消消息会 `disconnect` 活动连接；`onTrimMemory` 只取消当前窗口下载，Activity 销毁
  由宿主服务统一关闭剩余下载。

### `shell.openExternal`

```kotlin
val opened = KSM.shell.openExternal("https://example.com/docs")
```

`KSMAndroidHostServices(activity, allowedExternalOrigins)` 默认使用空白名单。origin
比较包含 scheme、host 和端口，默认端口规范化；未授权 origin 返回
`PERMISSION_DENIED`。只允许绝对 `http` / `https` URL，使用 `ACTION_VIEW`，不把任意
`content://`、`intent://` 或非 Web scheme 交给系统。

### `device.getInfo`

```kotlin
val info = KSM.device.getInfo()
```

返回 `platform`、`osVersion`、`model`、`manufacturer`、`appVersion`、`language`、
`screenWidth`、`screenHeight`、`density`、`deviceName`、`timezone` 和 `architecture`。
屏幕尺寸来自当前窗口度量的物理像素；`deviceName` 读取失败或为空时回退到 `Build.MODEL`。
不返回 Android ID、序列号、IMEI、广告 ID 或其他持久设备标识。

## 生命周期

示例 Activity 在 `onCreate` 创建文件系统、宿主服务和 `KSMWebViewHost`，在 `onDestroy`
销毁当前 WebView 并调用 `hostServices.shutdown()`。旋转会重建 Activity，旧请求的响应由
窗口注销和 Activity 销毁检查抑制。`onTrimMemory` 在低内存级别取消当前窗口下载，清理
WebView 内存缓存并释放可回收页面内存，保留磁盘缓存以避免反复重新下载资源。

文件选择器、通知权限和 Activity Result 都通过宿主服务等待结果；超时后清理 pending
状态。已有选择器未完成时，新的选择请求返回 `BUSY`，不会返回 `null` 或覆盖待处理状态。
取消和超时竞态由 Job、活动请求句柄和下载会话共同处理。访问文件选择器后产生的
`app://picked/<id>` 映射在宿主关闭时释放，不会跨 Activity 生命周期继续持有 URI。

Android O 及以上把 WebView 渲染进程设置为绑定当前应用，并允许系统在 WebView 不可见时
waive 渲染进程优先级，降低应用进入后台后继续占用 WebView 内存的概率。多个 WebView
共用进程级定时器暂停状态时使用租约计数，单个窗口恢复不会提前恢复其他窗口。

`KSMWebViewHost` 默认 `fullscreen = true`，隐藏 ActionBar、状态栏和导航栏，并把 WebView
扩展到系统栏与刘海区域。系统栏可通过边缘滑动临时显示，窗口重新获得焦点后会再次隐藏。
Activity 需要声明：

```xml
<activity
    android:name=".MainActivity"
    android:theme="@style/Theme.Kasumi.Fullscreen"
    android:windowSoftInputMode="adjustResize" />
```

如果应用需要保留系统栏，在创建宿主时传入 `fullscreen = false`。标题栏仍由
`Theme.Kasumi.Fullscreen` 移除。

## WebView 安全

WebView 的生产与开发入口由 `android.webview` 配置。默认生产入口是
`https://app.local/index.html`，开发入口是 `http://localhost:5173/index.html`、
`http://localhost:5173/` 与 `http://localhost:5173`。`KSMDevelopmentEntry.resolve()`
按 `KSMWebViewConfig` 校验 `ksmDevUrl`，只有 debug 构建且 URL 精确列入
`developmentEntries` 时才进入同一消息桥；否则回退生产入口。导航与消息来源按配置派生，
HTML 路径只有空路径、目录路径或 `supportedHtmlFiles` 中的文件才允许加载。其他
`http` / `https` 导航直接拦截，应用必须通过
`shell.openExternal` 的精确 origin 白名单打开外部链接；`intent:`、`file:` 等 scheme
同样拒绝。`window.open` 被拒绝，生产构建关闭 WebView 调试。CSP、导航拦截和来源校验
必须保持开启，不使用 `addJavascriptInterface`。

使用 `KSMWebAssetLoader.create(context, assetRoot, webViewConfig)` 创建资源加载器。该入口固定
域名 `app.local`，目录请求回退到 `config.productionHtmlFile`，并拒绝白名单外的 HTML
文件、路径穿越和编码路径。省略 `webViewConfig` 时使用 `KSMWebViewConfig.default()`。AndroidX 默认域名是
`appassets.androidplatform.net`，直接构造 `WebViewAssetLoader.Builder` 时漏设域名会让
`https://app.local/index.html` 落到系统 DNS 并报 `ERR_NAME_NOT_RESOLVED`。

```kotlin
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import dev.kasumi.app.bridge.KSMWebAssetLoader

val assetLoader = KSMWebAssetLoader.create(this, "web")
webView.webViewClient = object : WebViewClient() {
    override fun shouldInterceptRequest(
        view: WebView,
        request: WebResourceRequest
    ): WebResourceResponse? {
        return assetLoader.shouldInterceptRequest(request.url)
    }
}
webView.loadUrl("https://app.local/index.html")
```

Android 13 及以上的通知需要先调用 `permission.request` 请求 `notification.show`，
SDK 清单已声明 `POST_NOTIFICATIONS`。

## 测试

```bash
python cli/ksm.py test --platform android --target sdk
```

该命令构建 AAR，并运行内部测试宿主：

```bash
gradle -p android :kasumi:testDebugUnitTest :app:testDebugUnitTest :app:assembleDebugAndroidTest
```

| 测试 | 覆盖内容 |
| --- | --- |
| `ContractTest` | 根入口子模块、fail-closed 权限、symlink containment、picked、picker `BUSY`、双窗口路由、导航与 reload 事件、SQL 限额与取消、Store 并发、关闭后不可用 |
| `KSMHttpPolicyTest` | HTTPS、凭据、origin、端口、重定向和长度边界 |
| `NavigationPolicyTest` | WebView 来源和外部导航白名单 |
| `BridgeInstrumentedTest` | 路径守卫、大文本、事件、picked/bundled 资源、通知权限 |
| `KSMAndroidHostServicesTest` | 精确 origin 与设备字段 |
| `KSMWebViewRuntimeTest` | 全屏系统栏、安全开关、DOM storage、缓存模式、后台优先级和低内存回收入口 |

仍需在 API 34 模拟器和真机补测文件选择器、通知权限、旋转、`onTrimMemory`、下载取消
以及超时/取消竞态。

## 打包与签名

```bash
python cli/ksm.py package --platform android --target sdk
python cli/ksm.py package --platform android --target example
```

- Release SDK 产物：`.ksm/release/kasumi/<version>/kasumi-android-sdk-<version>.aar` 与
  `.ksm/release/kasumi/<version>/kasumi-android-maven-<version>.zip`。
- Release 示例产物：`.ksm/release/app/<version>/kasumi-android-example-<version>.apk`。

框架维护者测试宿主的 release 构建从以下环境变量读取签名材料：

| 环境变量 | 用途 |
| --- | --- |
| `KASUMI_ANDROID_KEYSTORE` | keystore 文件绝对路径 |
| `KASUMI_ANDROID_KEYSTORE_PASSWORD` | keystore 口令 |
| `KASUMI_ANDROID_KEY_ALIAS` | key alias |
| `KASUMI_ANDROID_KEY_PASSWORD` | key 口令 |

四个变量都有效且 keystore 存在时，release 使用该签名配置；否则构建保持未签名。
发布工作流还会使用 `apksigner verify --print-certs` 校验签名。

## 排查路径

1. 依赖解析失败：确认本地 Maven 仓库存在 `dev/kasumi/kasumi/<version>/`。
2. 页面空白：检查 `assets/web/index.html`、CSP、`KSMWebAssetLoader` 的资源根与
   `android.webview.supportedHtmlFiles`。
3. 开发模式回退生产入口：确认 `ksmDevUrl` 精确匹配 `developmentEntries`，且 APK 是
   debug 构建。
4. 请求一直超时：检查 `withTimeout`、专用执行器的取消链路、主线程回投和
   `timeoutMs` 是否在 `1..600000`。
5. 文件被拒绝：检查虚拟根、picked 句柄和真实 URI 是否越界。
6. 下载失败：检查 HTTPS、重定向、目标覆盖策略、网络权限和 `.part` 清理。
7. 外部链接被拒绝：检查 origin 白名单、scheme、host 和端口。
8. 通知不显示：先请求 `notification.show`，再检查 `POST_NOTIFICATIONS`。
9. Activity 旋转后异常：检查旧请求是否被取消，迟到响应是否被抑制。
10. 多 WebView 请求走错窗口：检查是否显式传入 `KSMWebViewHost.windowId`，不要把默认窗口
   上下文用于多窗口路由。
11. SQL 一直等待：确认 SQL request ID 未重复、取消令牌已透传，并检查
    `CancellationSignal` 是否触发。
