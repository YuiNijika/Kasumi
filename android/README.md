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
| `kasumi/src/main/java/dev/kasumi/app/bridge/KSMHttpPolicy.kt` | URL、origin 和重定向策略 |
| `kasumi/src/main/AndroidManifest.xml` | 网络与通知权限声明 |
| `app/src/main/java/dev/kasumi/app/MainActivity.kt` | 内部测试宿主的 WebView 与生命周期装配 |
| `app/src/test/` | JVM 契约与策略测试 |
| `app/src/androidTest/` | instrumentation 与真实 Activity 测试 |

Android 当前实现 **25/25 个适用命令**。Windows 专属的窗口标题、最小化、最大化、
尺寸调整不提供；`device.getInfo` 仅 Android 提供。

## SDK 构建

```bash
python cli/ksm.py build --platform android --target sdk
```

该命令执行：

```bash
gradle -p android :kasumi:assembleRelease
gradle -p android :kasumi:publishReleasePublicationToLocalRepository
```

AAR 位于 `android/kasumi/build/outputs/aar/kasumi-release.aar`，本地 Maven 仓库位于
`android/kasumi/build/repo`。消费者使用：

```kotlin
dependencies {
    implementation("dev.kasumi:kasumi:0.1.0")
}
```

独立示例工程位于 `examples/android-host/`，只消费 AAR，不包含框架 Kotlin 源码：

```bash
python cli/ksm.py build --platform android --target sdk
gradle -p examples/android-host assembleDebug
```

## 宿主入口

```kotlin
import dev.kasumi.app.KSMAndroidHostServices
import dev.kasumi.app.bridge.KSM
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMRouter

val fileSystem = KSMFileSystem(dataDir, cacheDir, tmpDir)
val hostServices = KSMAndroidHostServices(
    activity = this,
    allowedExternalOrigins = setOf("https://example.com"),
    fileSystem = fileSystem
)
val router = KSMRouter(fileSystem, hostServices) { event ->
    // Forward native events to the WebView
}

KSM.init(router, fileSystem, hostServices)
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

`WebViewCompat.addWebMessageListener` 接收消息后放入 IO 协程，最多同时执行 8 条命令。
超过上限返回 `BUSY`，超时返回 `TIMEOUT`，取消消息会中断任务并返回 `CANCELLED`。

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

示例 Activity 在 `onCreate` 创建文件系统、宿主服务和路由器，在 `onDestroy` 取消命令
协程、取消下载、关闭 `KSM` 并销毁 WebView。旋转会重建 Activity，旧请求的响应由
Activity 销毁检查抑制。`onTrimMemory` 在低内存级别取消活动下载并清理 WebView 缓存。

文件选择器、通知权限和 Activity Result 都通过宿主服务等待结果；超时后清理 pending
状态。取消和超时竞态由协程、`activeRequests`、`cancelledRequests` 和下载会话共同处理。

## WebView 安全

WebView 只允许 `https://app.local` 和开发用 `http://localhost:5173`。其他 `http` /
`https` 导航交给系统浏览器，`intent:`、`file:` 等 scheme 会被拒绝；`window.open`
被拒绝，生产构建关闭 WebView 调试。CSP、导航拦截和来源校验必须保持开启，不使用
`addJavascriptInterface`。

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

仍需在 API 34 模拟器和真机补测文件选择器、通知权限、旋转、`onTrimMemory`、下载取消
以及超时/取消竞态。

## 打包与签名

```bash
python cli/ksm.py package --platform android --target sdk
python cli/ksm.py package --platform android --target example
```

- SDK 产物：`android/build/packages/kasumi-android-sdk-0.1.0.aar`。
- 示例产物：`examples/android-host/build/packages/kasumi-android-example-0.1.0.apk`。

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

1. 依赖解析失败：确认本地 Maven 仓库存在 `dev/kasumi/kasumi/0.1.0/`。
2. 页面空白：检查 `assets/web/index.html`、CSP 和 `WebViewAssetLoader` 映射。
3. 请求一直超时：检查 `withTimeout`、`runInterruptible` 和主线程回投。
4. 文件被拒绝：检查虚拟根、picked 句柄和真实 URI 是否越界。
5. 下载失败：检查 HTTPS、重定向、目标覆盖策略、网络权限和 `.part` 清理。
6. 外部链接被拒绝：检查 origin 白名单、scheme、host 和端口。
7. 通知不显示：先请求 `notification.show`，再检查 `POST_NOTIFICATIONS`。
8. Activity 旋转后异常：检查旧请求是否被取消，迟到响应是否被抑制。
