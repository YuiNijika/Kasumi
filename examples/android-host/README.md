# Kasumi Android 示例宿主

> 一个独立 Android 应用工程，通过本地 Maven 中的 `dev.kasumi:kasumi:0.1.0` 使用 Kasumi SDK。

## 模块边界

本工程不包含 Kasumi 的 Kotlin 实现源码，也不通过 Gradle 源码替换依赖框架模块。

| 文件 | 责任 |
| --- | --- |
| `settings.gradle.kts` | 配置 Google、Maven Central 和本地 Kasumi Maven 仓库 |
| `app/build.gradle.kts` | 声明 `dev.kasumi:kasumi:0.1.0` 与共享 Web 资源 |
| `MainActivity.kt` | WebView、消息桥、协程、生命周期和导航策略 |
| `../showcase/` | Windows 与 Android 共用的完整能力页面 |

仓库内的 `android/app/` 是框架维护者的测试宿主，不是面向 SDK 消费者的模板。

## 构建

先发布 AAR 到 `android/kasumi/build/repo`：

```bash
python cli/ksm.py build --platform android --target sdk
```

再构建独立示例：

```bash
gradle -p examples/android-host assembleDebug
```

生成的 APK 位于 `examples/android-host/app/build/outputs/apk/debug/app-debug.apk`。

## 运行边界

- WebView 只加载 `https://app.local/index.html`，资源由 `WebViewAssetLoader` 提供。
- 外部链接白名单只包含 `https://example.com`。
- 下载目标限制在 `app://data`、`app://cache` 和 `app://tmp`。
- 文件选择器只向前端返回 `app://picked/<id>`。
- 设备信息不包含任何持久设备标识。

## 排查

1. 依赖解析失败：先确认本地 Maven 仓库存在 `dev/kasumi/kasumi/0.1.0/`。
2. 页面空白：检查 `assets.srcDir` 是否指向 `examples/showcase`。
3. 请求超时：确认 `WebViewCompat.addWebMessageListener` 的来源和 `ksm` 通道名称。
4. 外部链接被拒绝：检查 `allowedExternalOrigins` 的 scheme、host 和端口。
5. 生命周期异常：确认旧 Activity 销毁后不再投递迟到响应。
