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

Windows SDK 的 CMake package 依赖 `nlohmann-json` 与 `unofficial-webview2`。CLI
依次从 `CMAKE_TOOLCHAIN_FILE`、`VCPKG_ROOT` 和 Visual Studio 自带的 vcpkg 中查找
toolchain，并按项目内的 `windows/vcpkg.json` 安装依赖。vcpkg 的 registries、二进制
缓存和下载缓存都放在项目 `.ksm/vcpkg/` 下，避免用户目录的加密或权限设置导致
registry cache 创建失败。

Windows 批处理入口统一交给 `cmd.exe` 启动，因此 pnpm、Gradle Wrapper 等项目脚本
不依赖 Node 的 `shell` 选项。Android 构建优先使用项目自带 Wrapper，只有 Wrapper
缺失时才查找系统 Gradle。

## 配置

项目配置包含 Windows 与 Android 各自的包名、版本和命令 allowlist。Windows 还可设置
可选的 `decorations` 布尔值，默认 `true`；设为 `false` 会隐藏原生标题栏。`commands`
使用 `"all"` 时会按平台协议矩阵展开全部命令；显式数组仍会拒绝未知字段、非法包名、
非 SemVer 版本、未知命令和跨平台命令，并要求包含 `ping` 与 `capabilities.query`。
Android 不接受 `decorations` 字段。

`doctor` 使用与实际构建相同的框架识别逻辑。Next.js 与 Nuxt 项目只需要存在
`frontend/`、`frontend:dev` 和 `frontend:build`，不再误报缺少根级
`frontend/index.html`。Android release signing 在 `doctor` 中是提示项，不影响普通
debug 构建。

## 发布包

`ksm build` 输出可直接运行或调试的产物，`ksm package` 输出可分发包。两者都位于项目
`.ksm/dist/` 下：

| 命令 | 产物 |
| --- | --- |
| `ksm package --windows` | `.ksm/dist/packages/<packageName>-<version>-windows.zip` |
| `ksm package --android` | `.ksm/dist/packages/<packageName>-<version>-android.apk` |
| `ksm package --all` | 同时生成两个平台产物 |

Windows ZIP 默认包含 EXE 和 `README.txt`。传入 `--external-web` 会把
`frontend/dist/` 复制为旁边的 `web/`；传入 `--with-webview2-loader` 会在 Loader
可用时附带 `WebView2Loader.dll`。未启用外置 Web 时，资源继续嵌入 EXE。

Android release 构建读取四个签名变量：

```text
KASUMI_ANDROID_KEYSTORE
KASUMI_ANDROID_KEYSTORE_PASSWORD
KASUMI_ANDROID_KEY_ALIAS
KASUMI_ANDROID_KEY_PASSWORD
```

缺少任一项会在 Gradle 启动前失败。CLI 优先使用 `APKSIGNER`，否则从
`ANDROID_HOME` 或 `ANDROID_SDK_ROOT` 的 `build-tools` 中选择数值版本最高的
`apksigner`。只有 `apksigner verify` 通过后，APK 才会复制到 `.ksm/dist/packages/`。

## 开发服务器与来源

Windows 与 Android 生产宿主只接受 `https://app.local`。开发 origin
`http://localhost:5173` 只在 Windows `dev` 或 Android debug APK 中开放。
Android WebView 遇到非应用导航会直接取消，不会绕过白名单启动外部 Activity。

POSIX 下 CLI 以独立进程组启动前端开发服务器，退出时终止整个进程组，避免 Vite、
Next.js 或 Nuxt 的包管理器子进程残留。Windows 使用 `taskkill /T /F` 终止进程树。

常见失败行为：

| 现象 | 根因 | 处理 |
| --- | --- | --- |
| `Android release signing requires` | 签名变量缺失 | 补齐四个环境变量 |
| `apksigner was not found` | Android build-tools 不完整 | 安装 build-tools 或设置 `APKSIGNER` |
| `External web assets were not produced` | 外置 Web 构建失败 | 检查 CMake 的 `web/` 输出后重新打包 |
| `frontend entry` 为 missing | 项目缺少 `frontend/` | 恢复前端目录或重新初始化项目 |
