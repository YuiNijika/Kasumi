# __APP_TITLE__

> 一个由 `create-kasumi` 初始化的 Windows 与 Android 应用。

## 模块边界

项目只包含应用代码、平台宿主和 Kasumi SDK 消费配置。框架的 C++ 与 Kotlin 源码
不会复制到这里。

| 路径 | 责任 |
| --- | --- |
| `frontend/` | Vite 前端入口，使用 `kasumi-bridge` |
| `windows/` | Win32 + WebView2 应用宿主 |
| `android/` | Android WebView 应用宿主 |
| `vendor/windows/kasumi/` | 从 Release 下载的 Windows SDK |
| `vendor/android/repo/` | 从 Release 下载的 Android Maven 仓库 |
| `kasumi.config.json` | 两端包名、版本与命令 allowlist |

## 开始

```bash
pnpm install
pnpm ksm doctor
pnpm ksm dev --windows
pnpm ksm build --all
```

Android 开发前连接设备或启动模拟器，然后运行：

```bash
pnpm ksm dev --android
```

## 构建产物

```text
dist/windows/__WINDOWS_PACKAGE_NAME__.exe
dist/android/__WINDOWS_PACKAGE_NAME__-__KASUMI_VERSION__.apk
```

Windows 需要 CMake、Visual Studio C++ 工具链、vcpkg 和 Evergreen WebView2 Runtime。
Android 需要 JDK 21、Android SDK 和 Gradle。

## 排查

1. `kasumi.config.json` 非法：运行 `pnpm ksm doctor` 查看具体字段。
2. Windows 找不到 SDK：重新运行 `pnpm create kasumi`，或恢复 `vendor/windows/kasumi/`。
3. Windows 找不到依赖：设置 `VCPKG_ROOT`，确认 vcpkg 已安装 `webview2` 和 `nlohmann-json`。
4. Android 依赖解析失败：确认 `vendor/android/repo/` 内存在 `dev/kasumi/kasumi/`。
5. Android 页面空白：确认先完成 `frontend/dist/` 构建，并检查 `assets` 配置。
