# kasumi-cli

Kasumi 应用项目的构建与开发入口。生成项目通过根目录 `package.json` 调用它：

```bash
pnpm ksm dev
pnpm ksm build --windows
pnpm ksm build --android
pnpm ksm build --all
pnpm ksm package --all
pnpm ksm doctor
```

CLI 只读取项目内的 `kasumi.config.json`、`frontend/`、`windows/`、`android/` 和
`vendor/`，不会修改或复制 Kasumi 框架源码。

## 前端框架

CLI 读取根 `package.json` 中的 `frontend:dev` 和 `frontend:build` 脚本启动或构建前端。
内置识别以下框架，并把静态产物统一到 `frontend/dist/`：

| 依赖 | 开发地址 | 原始输出 |
| --- | --- | --- |
| Vite | `http://localhost:5173/index.html` | `frontend/dist/` |
| Next.js | `http://localhost:5173/` | `frontend/out/` |
| Nuxt | `http://localhost:5173/` | `frontend/.output/public/` |

旧版只包含 `frontend/vite.config.ts`、没有上述脚本的 Vite 项目继续使用兼容路径。
包管理器按 `packageManager` 字段或项目锁文件选择 `pnpm`、`npm` 或 `yarn`。

Next.js 与 Nuxt 的静态 HTML 包含框架生成的启动脚本。CLI 在缺失 CSP 时向
`frontend/dist/index.html` 注入 `script-src 'self' 'unsafe-inline'` 和
`style-src 'self' 'unsafe-inline'`，避免 Android WebView 出现空白页；现有 CSP 不会被
重复覆盖。导航、命令 allowlist、来源校验和虚拟路径限制仍由宿主执行。

## 前置条件

- Node.js 20 或更高版本
- Windows 构建需要 CMake、Visual Studio C++ 工具链和 vcpkg
- Android 构建需要 JDK 21、Android SDK；项目自带 Gradle Wrapper
- Android 开发运行还需要 `adb`

Windows SDK 的 CMake package 依赖 `nlohmann-json` 与 `unofficial-webview2`。设置
`VCPKG_ROOT` 或 `CMAKE_TOOLCHAIN_FILE` 后，CLI 会把 vcpkg toolchain 传给 CMake，
并按项目内的 `windows/vcpkg.json` 安装依赖。

Windows 批处理入口统一交给 `cmd.exe` 启动，因此 pnpm、Gradle Wrapper 等项目脚本
不依赖 Node 的 `shell` 选项。Android 构建优先使用项目自带 Wrapper，只有 Wrapper
缺失时才查找系统 Gradle。

## 配置

项目配置包含 Windows 与 Android 各自的包名、版本和命令 allowlist。Windows 还可设置
可选的 `decorations` 布尔值，默认 `true`；设为 `false` 会隐藏原生标题栏。`commands`
使用 `"all"` 时会按平台协议矩阵展开全部命令；显式数组仍会拒绝未知字段、非法包名、
非 SemVer 版本、未知命令和跨平台命令，并要求包含 `ping` 与 `capabilities.query`。
Android 不接受 `decorations` 字段。
