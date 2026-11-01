# Kasumi Android 示例宿主

> 一个独立 Android 应用工程，通过本地 Maven 中的 `dev.kasumi:kasumi:0.1.0` 使用 Kasumi SDK。

## 模块边界

本工程不包含 Kasumi 的 Kotlin 实现源码，也不通过 Gradle 源码替换依赖框架模块。

| 文件 | 责任 |
| --- | --- |
| `settings.gradle.kts` | 配置 Google、Maven Central 和本地 Kasumi Maven 仓库 |
| `app/build.gradle.kts` | 声明 `dev.kasumi:kasumi:0.1.0` 与共享 Web 资源 |
| `MainActivity.kt` | Activity 生命周期转发、宿主服务配置和 `KSMWebViewHost` 启动 |
| `../showcase/` | Windows 与 Android 共用的完整能力页面 |

仓库内的 `android/app/` 是框架维护者的测试宿主，不是面向 SDK 消费者的模板。

## 构建

先发布 AAR 到 `android/kasumi/build/repo`：

```bash
python cli/ksm.py build --platform android --target sdk
```

再构建独立示例：

```bash
cd examples/android-host
./gradlew assembleDebug
```

生成的 APK 位于 `examples/android-host/app/build/outputs/apk/debug/app-debug.apk`。
连接设备或启动模拟器后，可在仓库根目录运行 `pnpm ksm dev --android`。该命令会启动
`http://localhost:5173` 静态服务器，通过 `adb reverse` 把端口转发到设备，并安装、启动
示例应用。

应用 ID、版本和启用命令来自仓库根目录 `kasumi.config.json` 的 `android` 配置。
CLI 会通过 `-PksmPackageName`、`-PksmVersion` 和 `-PksmCommands` 注入 Gradle；
直接运行 Wrapper 时使用 Gradle 中的默认值。`commands` 生成
`BuildConfig.KSM_COMMANDS` 并交给 `KSMRouter`，未启用的命令返回 `NOT_SUPPORTED`。

## 运行边界

- WebView 只加载 `https://app.local/index.html`，资源由
  `KSMWebAssetLoader.create(context)` 提供。该入口固定域名 `app.local`；直接使用
  `WebViewAssetLoader` 默认域名 `appassets.androidplatform.net` 不会匹配该 URL。
- 外部链接白名单只包含 `https://example.com`。
- 下载目标限制在 `app://data`、`app://cache` 和 `app://tmp`。
- 文件选择器只向前端返回 `app://picked/<id>`。
- 设备信息不包含任何持久设备标识。
- `KSMWebViewHost` 负责 WebView 设置、消息桥、导航拦截、命令取消和资源释放。
- 默认使用 `Theme.Kasumi.Fullscreen` 并进入沉浸式全屏，隐藏标题栏、状态栏和导航栏。
- 应用进入后台时暂停 WebView 定时器；低内存时清理内存缓存并释放可回收内存，保留磁盘缓存。
- Android O 及以上允许系统在 WebView 不可见时 waive 渲染进程优先级。

## 排查

1. 依赖解析失败：先确认本地 Maven 仓库存在 `dev/kasumi/kasumi/0.1.0/`。
2. 页面报 `ERR_NAME_NOT_RESOLVED`：确认宿主使用 `KSMWebAssetLoader.create(context)`，
   且 `assets.srcDir` 指向 `examples/showcase`。
3. 请求超时：确认 `WebViewCompat.addWebMessageListener` 的来源和 `ksm` 通道名称。
4. 外部链接被拒绝：检查 `allowedExternalOrigins` 的 scheme、host 和端口。
5. 生命周期异常：确认旧 Activity 销毁后不再投递迟到响应。
6. 返回应用后页面定时器不恢复：确认 `onResume` 调用了 `KSMWebViewHost.onResume`。
7. 返回应用后内存仍偏高：检查是否存在额外 WebView，当前 Android 宿主只管理一个实例。
8. 状态栏重新出现：确认 Activity 转发 `onWindowFocusChanged`，或重新调用
   `KSMWebViewRuntime.applyFullscreen(activity)`。
