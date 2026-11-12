# __APP_TITLE__

> 一个由 `create-kasumi` 初始化的 Windows 与 Android 应用。

## 模块边界

项目只包含应用代码、平台宿主和 Kasumi SDK 消费配置。框架的 C++ 与 Kotlin 源码
不会复制到这里。

| 路径 | 责任 |
| --- | --- |
| `frontend/` | 前端入口，由所选框架构建并接入 `kasumi-bridge` |
| `windows/` | Win32 + WebView2 应用宿主 |
| `android/` | Android WebView 应用宿主 |
| `vendor/windows/kasumi/` | 从 Release 下载的 Windows SDK |
| `vendor/android/repo/` | 从 Release 下载的 Android Maven 仓库 |
| `kasumi.config.json` | 两端包名、版本、命令能力和 Windows 窗口配置 |

## 开始

```bash
pnpm install
pnpm ksm doctor
pnpm ksm dev --windows
pnpm ksm build --all
```

首次 Windows 开发会在 `.ksm/vcpkg/` 初始化依赖，之后复用安装、下载和二进制缓存。
`ksm dev` 不会重复构建生产前端，也不会在每次启动时重新配置 CMake。若上一次开发异常
退出，CLI 会自动清理 `.ksm/dev/state.json` 中记录的宿主与开发服务器。
`ksm dev --windows` 会监听 `kasumi.config.json`、`windows/` 和 `extensions/`，后端
源码或配置变化后自动重建并重启应用；前端页面仍使用开发服务器的热更新。

Android 开发前连接设备或启动模拟器，然后运行：

```bash
pnpm ksm dev --android
```

## 构建产物

```text
.ksm/dist/windows/__WINDOWS_PACKAGE_NAME__.exe
.ksm/dist/android/__ANDROID_PACKAGE_NAME__-__KASUMI_VERSION__.apk
```

Windows 需要 CMake、Visual Studio C++ 工具链、vcpkg 和 Evergreen WebView2 Runtime。
Android 需要 JDK 21 与 Android SDK。项目自带 Gradle Wrapper，不需要额外安装 Gradle。

默认配置使用 `"commands": "all"`。需要最小权限时改成显式数组，并保留 `ping` 与
`capabilities.query`。

### 自定义 Windows 标题栏

将 `kasumi.config.json` 中的 `windows.decorations` 设为 `false`，然后自行实现窗口按钮：

```ts
import { KSM } from "kasumi-bridge";

await KSM.window.minimize();
await KSM.window.maximize();
await KSM.window.close();
```

标题栏拖拽区域使用 `app-region: drag`，按钮和输入控件使用 `app-region: no-drag`。
原生缩放边框、最小尺寸和工作区约束仍由 Win32 宿主处理。Android 不接受 `decorations`。

### 窗口尺寸

`windows.window` 可设置初始尺寸和拖动边界，单位为 DIP：

```json
{
  "windows": {
    "window": {
      "width": 1280,
      "height": 800,
      "minWidth": 960,
      "minHeight": 600,
      "maxWidth": 1920,
      "maxHeight": 1200
    }
  }
}
```

`width` / `height` 省略或为 `0` 时回退 `1080` / `720`；`minWidth` / `minHeight`
回退 `320` / `240`；`maxWidth` / `maxHeight` 为 `0` 表示不限制。最大值必须不小于
最小值，初始尺寸不能超过最大值。

### WebView2 初始化与错误页

`windows.errorPage` 可以自定义 Runtime 缺失、WebView2 初始化失败或导航失败时显示的
原生错误面板。初始化加载态仍使用窗口背景对应的默认表面：

```json
{
  "windows": {
    "errorPage": {
      "backgroundColor": "#0B0B0D",
      "cardColor": "#15171D",
      "borderColor": "#2B2F38",
      "textColor": "#F5F6FA",
      "mutedTextColor": "#9CA3AF",
      "accentColor": "#8B5CF6",
      "errorColor": "#FF6B6B",
      "title": "应用启动失败",
      "hint": "请检查网络、WebView2 Runtime 或应用配置后重试"
    }
  }
}
```

颜色接受 `#RRGGBB` 或 `#AARRGGBB`，原生绘制会忽略 alpha；省略时按窗口背景色自动使用
浅色或深色默认配色。`title` 最长 80 个字符，`hint` 最长 240 个字符，只接受纯文本，
不能包含双引号、反斜杠、分号或控制字符。

### 应用图标

在 `kasumi.config.json` 的根级配置图标路径：

```json
{
  "icons": {
    "window": "icons/icon.ico",
    "tray": "icons/icon.ico",
    "installer": "icons/icon.ico",
    "android": "icons/android.png"
  }
}
```

Windows 图标必须是 `.ico`，Android 启动器图标必须是方形 `.png`。`window` 同时用于
EXE、标题栏和任务栏；`tray` 用于托盘，省略时回退到 `window`；`installer` 用于
NSIS/MSI 安装包。路径必须位于项目目录内。

## 排查

1. `kasumi.config.json` 非法：运行 `pnpm ksm doctor` 查看具体字段。
2. Windows 找不到 SDK：重新运行 `pnpm create kasumi`，或恢复 `vendor/windows/kasumi/`。
3. Windows 找不到依赖：设置 `VCPKG_ROOT`，CMake 会按 `windows/vcpkg.json` 自动安装 `webview2` 和 `nlohmann-json`。
4. Android 依赖解析失败：确认 `vendor/android/repo/` 内存在 `dev/kasumi/kasumi/`。
5. Android 页面空白：确认先完成 `frontend/dist/` 构建，并检查 `assets` 配置。
