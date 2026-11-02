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
| `kasumi/src/main/java/dev/kasumi/app/KSMAndroidHostServices.kt` | 对话框、文件、剪贴板、通知、下载、链接和设备信息 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMWebViewHost.kt` | WebView 消息桥、命令并发、导航策略、生命周期和资源释放 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMWebViewRuntime.kt` | WebView 默认设置、全屏窗口、前后台暂停和低内存回收 |
| `kasumi/src/main/res/values/styles.xml` | 无 ActionBar 的 `Theme.Kasumi.Fullscreen` Activity 主题 |
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMHttpPolicy.kt` | URL、origin 和重定向策略 |
| `kasumi/src/main/AndroidManifest.xml` | 网络与通知权限声明 |
| `app/src/main/java/dev/kasumi/app/MainActivity.kt` | 内部测试宿主的 WebView 与生命周期装配 |
| `app/src/test/` | JVM 契约与策略测试 |
| `app/src/androidTest/` | instrumentation 与真实 Activity 测试 |

Android 当前实现 **25/25 个适用命令**。Windows 专属的窗口标题、最小化、最大化、
尺寸调整不提供；`device.getInfo` 仅 Android 提供。

## 应用配置

Android 示例从仓库根目录 `kasumi.config.json` 读取 `android.packageName`、
`android.version` 和 `android.commands`：

```json
{
  "android": {
    "packageName": "dev.kasumi.example",
    "version": "0.1.0",
    "commands": [
      "ping",
      "capabilities.query",
      "app.getInfo",
      "device.getInfo",
      "fs.readText"
    ]
  }
}
```

`packageName` 注入 Gradle `applicationId`，同时用于 `pnpm ksm dev --android` 的
启动、停止和卸载目标；`version` 注入 `versionName`，示例再把
`BuildConfig.VERSION_NAME` 传给 `KSMAndroidHostServices`，因此 `app.getInfo.version`
与配置一致。`commands` 生成 `BuildConfig.KSM_COMMANDS` 并传入 `KSMRouter`，
`ping` 与 `capabilities.query` 必须保留；未列入的命令返回 `NOT_SUPPORTED`，Android
原生 `KSM` 子模块同样不能绕过配置。Gradle `namespace` 仍固定为源码包
`dev.kasumi.example`，避免仅修改应用 ID 时破坏 Kotlin 源码目录。

CLI 默认读取 `kasumi.config.json`，也可用 `--config <path>` 指定其他文件。直接运行
Gradle 时可用 `-PksmPackageName`、`-PksmVersion` 和逗号分隔的 `-PksmCommands`
覆盖默认值。

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
`shell`、`device`、`window` 和 `app`。直接子模块用于可信原生代码；前端命令仍必须经过
`KSMRouter` 的参数、权限和路径校验。

`KSMWebViewHost` 封装 `WebViewCompat.addWebMessageListener`，接收消息后放入 IO 协程，
最多同时执行 8 条命令。超过上限返回 `BUSY`，超时返回 `TIMEOUT`，取消消息会中断任务并
返回 `CANCELLED`。每个活动请求只保留一个可取消句柄，请求结束后立即移除，不维护无界
取消 ID 集合。

Activity 必须把 `onResume`、`onPause`、`onTrimMemory`、`onLowMemory` 和 `onDestroy`
转发给 `KSMWebViewHost`，并在窗口焦点恢复时转发 `onWindowFocusChanged`。宿主在前后台
切换时暂停或恢复 WebView 定时器，在低内存时只清理 WebView 内存缓存并调用 `freeMemory`，
不会删除磁盘缓存；销毁时移除消息监听、布局监听、协程和宿主服务引用。

## 文件与事件

`app://data`、`app://cache`、`app://tmp` 可读写；`app://res` 映射到 APK 的
`assets/res/`，`app://assets` 映射到 `assets/web/`，两者只读。`app://picked/<id>`
通过宿主保存的 `content://` 映射读写，真实 URI 不离开宿主层。

`fs.readDir` 返回按名称排序的目录项；`fs.mkdir` 只允许可写沙箱；`fs.stat` 支持可写
沙箱、只读资源和有效 picked 句柄。目录 `size` 为 `0`，`modifiedAt` 为 Unix 毫秒或
`null`。超过 64 KiB 的读取结果写入 `app://tmp/read-<id>.txt`，再次读取同一句柄返回
同一路径。

`webview.reloaded` 由 `onPageFinished` 作为系统事件发送；`window.resized` 由布局变化
发送，需要订阅，尺寸单位是物理内容区像素。

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
- 最多 5 次重定向，总超时 120 秒，最大 100 MiB。
- 目标必须是 `app://data`、`app://cache` 或 `app://tmp` 下的文件。
- 目标已存在且未允许覆盖时返回 `IO_ERROR`。
- 下载到同目录临时 `.part` 文件，成功后提交；失败、取消或异常时删除。
- 取消消息会 `disconnect` 活动连接；`onTrimMemory` 和 Activity 销毁会取消所有下载。

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
统一取消命令协程、下载和消息监听，关闭 `KSM` 并销毁 WebView。旋转会重建 Activity，
旧请求的响应由 Activity 销毁检查抑制。`onTrimMemory` 在低内存级别取消活动下载，清理
WebView 内存缓存并释放可回收页面内存，保留磁盘缓存以避免反复重新下载资源。

文件选择器、通知权限和 Activity Result 都通过宿主服务等待结果；超时后清理 pending
状态。取消和超时竞态由协程、活动请求句柄和下载会话共同处理。访问文件选择器后产生的
`app://picked/<id>` 映射在宿主关闭时释放，不会跨 Activity 生命周期继续持有 URI。

Android O 及以上把 WebView 渲染进程设置为绑定当前应用，并允许系统在 WebView 不可见时
waive 渲染进程优先级，降低应用进入后台后继续占用 WebView 内存的概率。Android 当前只
维护一个 Kasumi WebView 实例，因此暂停和恢复定时器按进程级行为处理。

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

WebView 只允许 `https://app.local` 和开发用 `http://localhost:5173`。其他 `http` /
`https` 导航交给系统浏览器，`intent:`、`file:` 等 scheme 会被拒绝；`window.open`
被拒绝，生产构建关闭 WebView 调试。CSP、导航拦截和来源校验必须保持开启，不使用
`addJavascriptInterface`。

使用 `KSMWebAssetLoader.create(context, assetRoot)` 创建资源加载器。该入口固定域名
`app.local`，并拒绝包含路径穿越的资源和请求路径。AndroidX 默认域名是
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
| `ContractTest` | 根入口子模块、文件、picked、权限、新 API、关闭后不可用 |
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
2. 页面空白：检查 `assets/web/index.html`、CSP 和 `KSMWebAssetLoader` 的资源根。
3. 请求一直超时：检查 `withTimeout`、`runInterruptible` 和主线程回投。
4. 文件被拒绝：检查虚拟根、picked 句柄和真实 URI 是否越界。
5. 下载失败：检查 HTTPS、重定向、目标覆盖策略、网络权限和 `.part` 清理。
6. 外部链接被拒绝：检查 origin 白名单、scheme、host 和端口。
7. 通知不显示：先请求 `notification.show`，再检查 `POST_NOTIFICATIONS`。
8. Activity 旋转后异常：检查旧请求是否被取消，迟到响应是否被抑制。
