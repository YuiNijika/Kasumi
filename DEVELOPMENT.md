# Kasumi 开发入口

> 从协议契约、前端桥接、Windows 宿主到 Android 宿主的开发顺序、当前进度、验证入口与排查路径。

## 模块边界

| 模块 | 入口 | 负责内容 |
| --- | --- | --- |
| 应用配置 | `kasumi.config.json` | 两端包名、版本、命令 allowlist 和 Windows 标题栏开关 |
| 协议 | `protocol/commands.json` | 命令、事件、参数、返回结构、权限和平台能力 |
| 代码生成 | `tools/codegen.py` | 生成 TypeScript、C++、Kotlin 契约常量与参数校验器 |
| 前端 | `frontend/src/index.ts` | `KSM` 根入口、事件总线和 API 封装 |
| Windows 根入口 | `windows/src/commands/ksm_phase1.h` | 宿主侧 `KSM` 生命周期、统一调用和原生事件 |
| Windows 路由 | `windows/src/bridge/ksm_router.h` | 请求校验、权限检查和命令分发 |
| Windows 窗口框架 | `windows/src/platform/ksm_window_frame.h` | 原生标题栏隐藏、缩放边框和工作区处理 |
| Windows WebView2 适配 | `windows/src/platform/ksm_webview2_host.h` | 共享 Environment、单窗口多 View、内嵌资源、消息隔离和 UI 线程回投 |
| Windows SDK 包 | `windows/CMakeLists.txt` | 静态库、公共头文件、CMake package config 和 CPack |
| Windows 示例 | `windows/examples/win32-host/` | 独立消费者工程，使用已安装 SDK |
| Android SDK | `android/kasumi/` | AAR、宿主服务、路由、生成契约和公开 API |
| Android 示例 | `examples/android-host/` | 独立消费者工程，使用本地 Maven 中的 AAR |
| Android 测试宿主 | `android/app/` | 框架维护者的 JVM 与 instrumentation 测试宿主 |
| 契约测试 | `tests/contract/run.ts` | 前端与双平台模拟宿主的共享行为测试 |
| CLI | `cli/ksm.py` | 协议校验、代码生成、双端构建、测试和打包入口 |
| 包管理入口 | `package.json` | 唯一应用依赖、TypeScript、契约测试和 `pnpm ksm` 命令入口 |
| 项目初始化 | `packages/create-kasumi/` | 发布为 `create-kasumi`，生成前后端分离的应用项目 |
| 应用 CLI | `packages/kasumi-cli/` | 发布为 `kasumi-cli`，在生成项目中提供 `ksm` 命令 |
| 前端桥接 | `packages/kasumi-bridge/` | 发布为 `kasumi-bridge`，提供 `KSM` TypeScript API |

协议字符串不加 `KSM` 前缀；宿主侧类型和入口统一使用 `KSM` 前缀。

SDK 与示例必须分开构建。消费者只接收静态库、公共头文件、AAR 或 Maven 坐标，不把
`windows/src/*.cpp` 或 `android/kasumi/src/**/*.kt` 复制进应用工程。

## 当前进度

当前协议基线为 **29 个命令、2 个事件**，产品版本保持 `0.1.0`。新增命令从
`since: "1.1.0"` 开始记录，命令名和参数以 `protocol/commands.json` 为准。

| 平台 | 适用命令 | 已实现 | 说明 |
| --- | ---: | ---: | --- |
| Windows | 28 | 28 | 不含仅 Android 提供的 `device.getInfo` |
| Android | 25 | 25 | 不含 Windows 专属的窗口标题、最小化、最大化、尺寸调整 |
| 协议全集 | 29 | 29 | 两端按平台矩阵过滤，不伪造跨平台能力 |

本轮新增：

- `http.download`：HTTPS 下载到可写虚拟路径，支持取消、120 秒总超时和 100 MiB 上限。
- `shell.openExternal`：只允许宿主显式配置的精确 origin，使用系统浏览器打开。
- `device.getInfo`：仅 Android 提供非持久设备、显示和运行时信息。
- `window.setSize`：仅 Windows 提供逻辑外框尺寸调整，最小 `320 × 240`。

前端 `KSM` 根入口已挂载 `http`、`shell`、`device` 和 `window.setSize`。Windows 与
Android 宿主侧根入口分别挂载对应子模块；Android 不提供 Windows 专属窗口方法。

发布边界已完成：

- Windows SDK 产出 `kasumi-windows-sdk-0.1.0.zip`，示例单独产出可执行程序 ZIP。
- Android SDK 产出 `kasumi-android-sdk-0.1.0.aar`，示例单独产出 APK。
- `windows/examples/win32-host/` 通过 `find_package(Kasumi CONFIG REQUIRED)` 消费 SDK。
- `examples/android-host/` 通过 `dev.kasumi:kasumi:0.1.0` 消费本地 Maven 中的 AAR。
- Windows SDK 包不包含 `.cpp`，Android Maven 发布不生成 sources JAR。

## 应用配置

根目录 `kasumi.config.json` 是 Windows 和 Android 共用的应用配置入口。当前只保留平台
包名、版本、启用命令和 Windows 标题栏开关，不引入 Tauri 的图标、权限或打包扩展字段。

```json
{
  "$schema": "./protocol/kasumi.config.schema.json",
  "windows": {
    "packageName": "kasumi-win32-example",
    "version": "0.1.0",
    "commands": "all",
    "decorations": true
  },
  "android": {
    "packageName": "dev.kasumi.example",
    "version": "0.1.0",
    "commands": "all"
  }
}
```

| 平台 | 字段 | 作用 |
| --- | --- | --- |
| Windows | `packageName` | EXE 版本资源中的内部名称和打包目录名 |
| Windows | `version` | EXE 文件版本、产品版本、打包文件名和 `app.getInfo.version` |
| Windows | `commands` | 注入 `KSM::Init` 的命令 allowlist，并过滤 `capabilities.query` |
| Windows | `decorations` | 是否显示原生标题栏，默认 `true`；`false` 使用页面自定义标题栏 |
| Android | `packageName` | Gradle `applicationId` 与 `adb` 启动、停止、卸载目标 |
| Android | `version` | Gradle `versionName` 与 `app.getInfo.version` |
| Android | `commands` | 注入 `KSMRouter` 的命令 allowlist，并过滤 `capabilities.query` |

CLI 默认读取仓库根目录的 `kasumi.config.json`。其他路径通过 `--config <path>` 指定。
`commands` 可写为 `"all"`，按当前平台协议矩阵展开全部命令；也可以写非空数组，只启用
显式列出的命令。`build`、`test`、`package` 和 `dev` 都会先校验配置。缺失字段、未知字段、
重复命令、空数组、未知命令、跨平台命令、非法 Android 包名或非 SemVer 版本会直接失败。
数组模式仍要求包含 `ping` 与 `capabilities.query`。

`windows.decorations: false` 只移除系统标题栏，不创建窗口控件。应用需要通过
`KSM.window.minimize()`、`KSM.window.maximize()` 和 `KSM.window.close()` 实现标题栏操作，
并在拖拽区域使用 `app-region: drag`，在按钮和输入控件上使用 `app-region: no-drag`。
Win32 宿主保留可缩放边框，因此边缘缩放、最小尺寸和工作区夹取行为不变。Android 不接受
`decorations` 字段。

已知但未列入 allowlist 的命令返回 `NOT_SUPPORTED`，不会继续进入参数校验或宿主服务。
Windows 的 `KSM` 原生便捷方法、Android 的 `KSM.fs` 等原生子模块和前端桥接都遵守同一份
allowlist。JSON Schema 位于 `protocol/kasumi.config.schema.json`。

## 协议与生成

新增或修改命令时按以下顺序执行，不得手写平行命令清单：

1. 修改 `protocol/commands.json`、`protocol/commands.schema.json` 和 `protocol/permissions.schema.json`。
2. 运行 `python tools/validate_protocol.py`。
3. 运行 `python tools/codegen.py`，同步 `frontend/src/generated/contract.ts`、`windows/src/generated/contract.h` 和 `android/kasumi/src/main/java/dev/kasumi/app/generated/KSMGeneratedContract.kt`。
4. 实现 Windows `KSMRouter`、Android `KSMRouter` 和宿主服务。
5. 增加前端 API、契约测试和平台测试。

`capabilities.query` 返回宿主实际支持的 `commands` 和已授予的 `permissions`。前端调用
`KSM.get()` 读取缓存能力；页面重载后 `KSM` 会清理缓存并重新查询。

## 新增 API 的固定边界

### `http.download`

```ts
const result = await KSM.http.download({
  url: 'https://example.com/archive.zip',
  path: 'app://data/archive.zip',
  overwrite: false
});
```

- 只接受 `https://`，拒绝凭据、无主机、端口异常和超长 URL。
- 目标是 `app://data`、`app://cache` 或 `app://tmp` 下的文件，不接受目录、`app://picked` 和真实路径。
- `overwrite` 缺省为 `false`；目标存在时返回 `IO_ERROR`。
- 最多跟随 5 次 HTTPS 重定向；每一跳都重新校验 HTTPS。
- 总超时 120 秒，最大 100 MiB；下载到同目录 `.part` 文件，成功后提交，失败或取消时清理。
- 不提供进度事件；前端超时会发送现有 `cancel` 消息，宿主必须中止网络请求。

### `shell.openExternal`

```ts
const result = await KSM.shell.openExternal({
  url: 'https://example.com/docs'
});
```

- 只接受绝对 `http` / `https` URL，拒绝用户名密码、`javascript:`、`file:`、`data:` 和 `intent:`。
- origin 比较包含 scheme、host 和显式端口，默认端口规范化。
- 宿主构造时传入精确 origin 白名单，默认空白名单，未授权返回 `PERMISSION_DENIED`。
- 只校验调用时传入的 URL；系统浏览器后续重定向不经过 Kasumi。

### `device.getInfo`

```ts
const info = await KSM.device.getInfo();
console.log(info.model, info.screenWidth, info.density);
```

仅 Android 可用。返回 `platform`、`osVersion`、`model`、`manufacturer`、`appVersion`、
`language`、`screenWidth`、`screenHeight`、`density`、`deviceName`、`timezone` 和
`architecture`。不返回 Android ID、序列号、IMEI、广告 ID 或其他持久设备标识。

### `window.setSize`

```ts
await KSM.window.setSize({ width: 960, height: 720 });
```

仅 Windows 可用。参数是窗口外框逻辑像素，最小 `320 × 240`；按当前 DPI 转成物理像素，
夹取到当前显示器工作区。最大化、最小化和全屏状态返回 `NOT_SUPPORTED`，不会自动恢复。
成功后由 `WM_SIZE` 继续发送 `window.resized`，事件尺寸单位是物理内容区像素。

## 宿主实现

### Windows

- `KSMWindowsHostServices` 提供 WinHTTP 下载、`ShellExecuteW` 外部链接和 Win32 窗口尺寸服务。
- `KSMWebView2Host` 负责窗口级 WebView2 Environment、共享内嵌或外置资源响应、消息来源校验、导航与新窗口拦截。
- `KSMWebView2View` 持有独立子 `HWND`、路由器、宿主服务、请求队列、超时计时器、尺寸、可见性和焦点。
- 每个 View 分配单调递增且不复用的 `windowId`；关闭一个 View 不影响同一顶层窗口的其他 View。
- `Initialize()` 保留单视图兼容入口；`InitializeSession()` 加 `CreateView()` 支持标签页、分屏和浏览器式多实例布局。
- `CreateView()` 和 `SetBounds()` 在夹取前拒绝无面积边界，避免零尺寸输入被转换成 `1×1`。
- `--multi-view` 将 `KASUMI_EXAMPLE_MULTI_VIEW` 设为 `ON`，普通启动直接加载
  `multi-view.html` 左右双实例演示；窗口 `WM_SIZE` 会重新分配两个 View 的边界。
- 双实例演示各自维护 DOM、滚动位置、请求队列和消息路由，只通过
  `app://data/multi-view-demo/shared-note.txt` 共享便签；`--multi-view-smoke` 使用同一套页面验证真实路由。
- `windows/` 只构建 SDK；`windows/examples/win32-host/` 是独立消费者，加载 `https://app.local/index.html`。
- 默认构建把 `examples/showcase/` 编译进 EXE，输出目录不包含 `web/`；WebView2 Loader 静态链接，用户数据写入 `%LOCALAPPDATA%\Kasumi\Example\WebView2`。
- `KASUMI_EXAMPLE_EXTERNAL_WEB=ON` 切换为外置 `web/`，命令行等价参数为 `--external-web`；运行时可用 `--web-root` 和 `--user-data-folder` 覆盖路径。
- SDK 安装目录包含 `include/kasumi/`、`kasumi_windows_bridge.lib` 和 `lib/cmake/Kasumi/`。
- 示例依赖系统 Evergreen WebView2 Runtime，缺失时显示安装提示并退出；默认不生成 `WebView2Loader.dll` 或 `kasumi_win32_example.exe.WebView2`，ZIP 也不内置 Fixed Version Runtime。
- `--with-webview2-loader` 或 `KASUMI_BUNDLE_WEBVIEW2_LOADER=ON` 才会使用动态 Loader 并在输出目录生成、打包 `WebView2Loader.dll`。
- 标签发布工作流要求 Authenticode 证书，使用 SHA-256 时间戳签名并执行 `signtool verify /pa /all`。

### Android

- `android/kasumi/` 是发布 AAR 的 Android Library 模块；`android/app/` 只保留维护者测试宿主。
- 本地 Maven 仓库位于 `android/kasumi/build/repo`，示例通过 `dev.kasumi:kasumi:0.1.0` 解析。
- `examples/android-host/` 是独立消费者工程，不通过 Gradle 源码替换依赖框架模块。
- `KSMAndroidHostServices` 使用 `HttpsURLConnection` 下载、`ACTION_VIEW` 打开外部链接，并从 `Build`、窗口度量、资源和时区读取设备信息。
- WebView 使用 `WebViewCompat.addWebMessageListener`，命令在 IO 协程中执行，最多 8 条并发。
- `onTrimMemory` 会取消活动下载；Activity 销毁、旋转和窗口关闭会停止协程、关闭下载并抑制迟到响应。
- 通知权限通过 `permission.request` 和 `POST_NOTIFICATIONS` 处理。

## 验证状态

| 检查 | 当前结果 |
| --- | --- |
| `python tools/validate_protocol.py` | 通过，输出 `protocol valid` |
| `python tools/codegen.py` | 通过，生成契约已同步 |
| 前端 TypeScript 类型检查 | 通过 |
| 测试工程 TypeScript 类型检查 | 通过 |
| `tests/contract/run.ts` | `frontend contract passed for windows and android` |
| `pnpm ksm build` | 默认 Windows 构建通过 |
| `pnpm ksm build --android` | Android SDK 与示例构建通过 |
| `pnpm ksm build --all` | Windows 与 Android SDK 构建通过 |
| `pnpm ksm build --all --target example` | 双端示例按 `kasumi.config.json` 注入包名、版本和命令 |
| `pnpm ksm build --windows --target example` | 默认输出仅 EXE，无 `WebView2Loader.dll` 或 `.WebView2` |
| `pnpm ksm build --windows --target example --multi-view` | 双 View 演示编译通过，普通启动加载 `multi-view.html` |
| `pnpm ksm build --windows --target example --with-webview2-loader` | 可选输出包含 `WebView2Loader.dll` |
| `pnpm ksm package --windows --target example` | 默认 ZIP 仅包含 EXE |
| `pnpm ksm dev` | Windows 宿主加载 `http://localhost:5173/index.html` |
| `pnpm ksm dev --android` | 模拟器通过 `adb reverse` 加载开发页面 |
| `pnpm ksm test --all --target sdk` | 双端契约、配置校验、allowlist、JVM 测试和示例 smoke test 通过 |
| `pnpm test:packages` | 19 个包测试通过，覆盖初始化、命名、批处理入口、Gradle Wrapper、协议命令矩阵和 `decorations` 配置 |
| `pnpm build:bridge` | `kasumi-bridge` 生成 ESM 与 TypeScript 声明 |
| `pnpm --filter create-kasumi pack` | npm tarball 包含模板、Gradle Wrapper、vcpkg manifest、命令数据和 bin 入口 |
| `npm exec --package create-kasumi-0.1.0.tgz` | 从实际 npm tarball 初始化项目通过 |
| 生成项目 `pnpm typecheck` | 模板 TypeScript 配置通过 |
| 生成项目 `vite build` | 前端构建输出到 `frontend/dist/` 通过 |
| 本地 Release HTTP 端到端 | 下载 manifest、校验 SHA-256、解压 Windows SDK 与 Android Maven 仓库通过 |
| Windows SDK ZIP 布局 | CPack 输出扁平目录，兼容旧版单顶层目录自动展开 |
| 生成项目 Windows 构建 | `pnpm ksm build --windows` 产出 EXE，vcpkg 按 manifest 自动安装依赖 |
| 生成项目 Windows smoke test | WebView2 加载、前端 `ping`、`app.exit` 后 2.3 秒内退出 |
| 生成项目 Android 构建 | JDK 21 + 自带 Gradle Wrapper 产出 debug APK |
| Windows 批处理入口 | `pnpm.cmd` 与 `gradlew.bat` 均通过 `cmd.exe` 启动，不再触发 `spawn EINVAL` |
| `pnpm ksm package --android --target sdk` | AAR 与 `kasumi-android-maven-0.1.0.zip` 生成通过 |
| `kasumi_win32_example_smoke` | Windows 示例启动、ping 与退出通过 |
| `kasumi_win32_example_multi_view_smoke` | 实际加载 `multi-view.html` 两侧页面，验证同窗口双 View、独立 windowId/子窗口/边界和命令路由、顶层窗口缩放重排、单 View 关闭后剩余实例继续运行 |
| 多 View 边界回归 | 创建零面积 View 被拒绝；对已存在 View 调用无面积 `SetBounds()` 失败且布局不变 |
| Windows 标题栏构建 | `ON` 与 `OFF` 均编译并通过 smoke test；`OFF` 移除 `WS_CAPTION`，客户区覆盖完整窗口，四边与四角返回对应缩放命中值 |
| Windows SDK CTest | 标题栏 `ON` 与 `OFF` 构建目录均 2/2 通过，含单 View 和多 View smoke |
| Windows 原生契约 | `kasumi_contract_runner.exe` 输出 `windows contract passed` |
| `node --test packages/*/tests/*.test.mjs` | 19/19 通过 |
| `python -m unittest tests.test_kasumi_config` | 14/14 通过 |
| TypeScript 双工程检查 | `frontend/tsconfig.json` 与 `tests/tsconfig.json` 均通过 |
| `git diff --check` | 通过；仅有仓库既有的 LF/CRLF 转换警告 |

Windows 已完成 SDK、内嵌 Web 示例构建和真实桌面 smoke test，`kasumi_contract`、
`kasumi_win32_example_smoke` 与 `kasumi_win32_example_multi_view_smoke` 均通过。
同一顶层窗口可创建多个共享 Environment、但路由和生命周期互相独立的 WebView View。
默认示例输出只有 EXE，没有 `WebView2Loader.dll`、
`kasumi_win32_example.exe.WebView2`、`web/` 或 `KasumiData/`。SDK ZIP 只包含公共头文件、
静态库、CMake 配置和 README；默认示例 ZIP 只包含 EXE，显式启用 Loader 打包时才额外包含
`WebView2Loader.dll`，外置模式 ZIP 额外包含 `web/`。

Android 已完成 7 个 JVM 测试、AAR 发布、维护者 APK、instrumentation 测试 APK 和独立
示例 APK 构建。AAR 和本地 Maven 发布物不包含 Kotlin 源码或 sources JAR，示例 APK 的
Web 资源位于 `assets/`。本机 JDK 的 `TEMP` 包含短文件名时，Gradle 的 AF_UNIX 连接会
报告 `Unable to establish loopback connection`，需要显式指定 socket 目录。发布版
`kasumi-cli` 会把项目内 `.ksm/afunix` 解析为绝对路径后写入
`jdk.net.unixdomain.tmpdir`，并优先使用模板自带的 Gradle 8.14.3 Wrapper。
API 34 模拟器已完成 3 项 connected tests，并验证示例页面实际加载。Android 宿主统一使用
`KSMWebAssetLoader.create(context, assetRoot)`；该入口固定域名 `app.local`，避免
`WebViewAssetLoader` 默认域名不匹配导致 `ERR_NAME_NOT_RESOLVED`。

仍需在真机完成以下检查：

1. Android 下载取消、通知权限、文件选择器、旋转和 `onTrimMemory` 的真实设备验证。
2. Windows Evergreen Runtime 缺失提示和标签签名流程。

## pnpm 命令入口

根目录 `package.json` 是应用项目的 JavaScript/TypeScript 依赖和命令入口。`frontend/`
只保留 Web 源码与 `tsconfig.json`，不再维护独立的 `package.json`、锁文件或
`node_modules`。`packages/*/package.json` 只用于发布独立工具包，不参与应用运行时。
底层命令仍调用 `cli/ksm.py`：

```bash
pnpm ksm build
pnpm ksm build --windows
pnpm ksm build --android
pnpm ksm build --all
pnpm ksm build --windows --with-webview2-loader
pnpm ksm build --all --target example --config kasumi.config.json
pnpm ksm dev
pnpm ksm dev --android
pnpm dev
pnpm dev:windows
pnpm dev:android
pnpm build
pnpm build:windows
pnpm build:android
pnpm build:all
pnpm test
pnpm typecheck
```

`build` 默认只构建 Windows；`--all` 同时构建 Windows 和 Android。旧的
`--platform windows|android|all` 继续可用。`dev` 默认启动 Windows 宿主，Android
开发模式会启动 `http://localhost:5173` 静态服务器、执行 `adb reverse`、安装示例 APK
并从该地址加载页面。所有应用构建命令默认读取根目录 `kasumi.config.json`。

Windows 默认使用 vcpkg 的 `x64-windows-static-md` triplet，静态链接 WebView2 Loader，
因此输出目录不需要 `WebView2Loader.dll`，运行时使用系统已安装的 Evergreen Runtime。
需要随应用分发 Loader 时传入 `--with-webview2-loader`，该模式使用 `x64-windows`
triplet，并写入独立的 `windows/build/sdk-webview2-loader` 与
`windows/examples/win32-host/build-webview2-loader` 目录。

如果 Python 不在 `PATH`，设置 `KASUMI_PYTHON` 指向 Python 3.10+ 可执行文件。

## 创建应用与发布包

发布后的用户入口与 Tauri 类似：

```bash
pnpm create kasumi
pnpm create kasumi my-app
pnpm create kasumi my-app --all
```

`create-kasumi` 只生成应用代码和平台宿主，不会把 `windows/src/*.cpp` 或
`android/kasumi/src/**/*.kt` 复制到用户项目。生成结构保持一份根 `package.json`：

```text
my-app/
├─ package.json
├─ kasumi.config.json
├─ frontend/
├─ windows/
├─ android/
└─ vendor/
   ├─ windows/kasumi/
   └─ android/repo/
```

初始化器从 GitHub Release 下载两个 SDK 资产，并校验 Release manifest 中的 SHA-256：

| 资产 | 用途 |
| --- | --- |
| `kasumi-windows-sdk-<version>.zip` | Windows CMake package、公共头文件和静态库 |
| `kasumi-android-maven-<version>.zip` | Android AAR 及其 POM/module 元数据 |
| `kasumi-release.json` | 版本、文件名、大小和 SHA-256 |

Windows SDK ZIP 使用扁平布局，解压后直接得到 `include/`、`lib/` 和 `README.md`。
初始化器仍兼容旧版带单个顶层目录的 ZIP，避免历史 Release 无法安装。

用户项目安装三个 npm 包：

| npm 包 | 作用 |
| --- | --- |
| `kasumi-bridge` | 前端 `KSM` API 与事件总线 |
| `kasumi-cli` | `ksm dev/build/package/test/doctor` |
| `create-kasumi` | 仅用于 `pnpm create kasumi`，不进入应用运行时 |

生成项目通过 `pnpm ksm build --windows`、`pnpm ksm build --android` 或
`pnpm ksm build --all` 构建。Windows 默认将 `frontend/dist/` 嵌入 EXE；Android
将 `frontend/dist/` 作为 `assets/` 打包。两者都不复制框架源码。

发布前先验证包内容：

```bash
pnpm test:packages
pnpm build:bridge
pnpm --filter kasumi-cli pack
pnpm --filter kasumi-bridge pack
pnpm --filter create-kasumi pack
```

标签发布工作流使用 `NPM_TOKEN` 发布三个 npm 包，并在 GitHub Release 中上传
Windows SDK、Android Maven 仓库、AAR、示例 ZIP、APK 和 `kasumi-release.json`。
正式标签发布仍要求 Windows Authenticode 与 Android APK 签名。

当前 npm registry 上 `create-kasumi`、`kasumi-cli` 和 `kasumi-bridge` 均未占用；
本机 npm 尚未登录，因此本轮只完成 tarball 与模拟 Release 的端到端验证，没有执行
真实 npm 或 GitHub Release 发布。

## 常用命令

```bash
pnpm ksm build --windows --target sdk
pnpm ksm build --android --target example
pnpm ksm build --windows --target example --with-webview2-loader
pnpm ksm build --windows --target example --multi-view
pnpm ksm dev
pnpm ksm dev --android
pnpm typecheck
pnpm test:contract
python -m pip install -e .
python tools/validate_protocol.py
python tools/codegen.py
python cli/ksm.py build --platform windows --target sdk
python cli/ksm.py build --platform windows --target example
python cli/ksm.py build --platform android --target sdk
python cli/ksm.py build --platform android --target example
python cli/ksm.py test --platform all --target sdk
python cli/ksm.py test --platform all --target example
python cli/ksm.py package --platform windows --target sdk
python cli/ksm.py package --platform windows --target example
python cli/ksm.py package --platform windows --target example --external-web
python cli/ksm.py package --platform android --target sdk
python cli/ksm.py package --platform android --target example
```

Windows 构建需要 vcpkg 的 `webview2` 与 `nlohmann-json` 依赖。默认 SDK 安装到
`windows/build/sdk/install`，依赖安装到 `.tmp/vcpkg-installed`；Loader 模式安装到
`.tmp/vcpkg-installed-webview2-loader`。默认示例使用 EXE 内嵌 Web 资源，不需要把
`web/` 放到输出目录。

Android 构建使用 JDK 21。若 Gradle 在本机启动 daemon 时报告 loopback 连接失败，将
`jdk.net.unixdomain.tmpdir` 指向不含短文件名的可写目录。`cli/ksm.py` 在 Windows 上会
自动设置 `.tmp/afunix`，并在当前 JDK 不受支持时查找同级的 `jdk-21`。也可以通过
`KASUMI_JAVA_HOME` 显式指定 JDK。手动调用 Gradle 时可使用：

```bash
JAVA_TOOL_OPTIONS="-Djdk.net.unixdomain.tmpdir=<workspace>/.tmp/afunix" \
gradle -p android :kasumi:assembleRelease :kasumi:publishReleasePublicationToLocalRepository
```

## 安全边界

- 前端只使用 `app://` 虚拟路径，不直接访问真实文件路径。
- 文件选择器返回 `app://picked/<id>`，不向前端暴露真实路径或 `content://` URI。
- 路径先拒绝危险输入，再做虚拟根解析；不得先规范化再放行。
- 单条 JSON 消息限制为 64KB，单文件读写限制为 16MB。
- 下载目标只允许可写沙箱，外部链接必须通过精确 origin 白名单。
- Android 不使用 `addJavascriptInterface`，保持 CSP、导航拦截和 `window.open` 拦截。
- Windows 生产环境关闭 DevTools，并校验 WebView2 消息来源。

## 排查路径

1. 确认命令已写入 `protocol/commands.json`，并运行协议校验和代码生成。
2. 确认生成文件包含命令、权限、事件和参数校验器。
3. 确认 `capabilities.query` 返回当前平台实际支持的命令。
4. 确认请求经过参数校验、权限校验和路径守卫。
5. 确认每个请求只返回一次响应，超时和取消会中止宿主任务。
6. 确认 WebView 回调回到 UI 线程或主线程，且销毁后不再投递迟到响应。
7. 确认事件来源是当前窗口；`webview.reloaded` 是系统事件，`window.resized` 需要订阅。
8. 下载失败时检查 URL scheme、重定向、目标是否存在、`.part` 清理和宿主取消链路。
9. 外部链接失败时检查 origin 白名单、端口、scheme 和系统是否安装浏览器处理器。

## 在线文档同步

在线 API 文档按模块拆分：`kasumi/api/app`、`capabilities`、`fs`、`dialog`、`clipboard`、
`notification`、`permission`、`event`、`window`、`http`、`shell`、`device` 和 `types`。
`kasumi/api` 只做索引，`kasumi/development` 记录当前实现进度。

此前 10 个线上修订已覆盖 `kasumi/development`、`kasumi/index`、`kasumi/api`、`fs`、
`types`、`window`、`http`、`shell`、`device` 和 `event`，当前状态为 `approved`。
本轮又提交了 `kasumi/development` 修订 `bf80e59e-76c8-4b00-b2f9-342908f31352`，
补充当前实现进度、命令矩阵和 Android `app.local` 资源加载修复，状态已变为
`approved`，但重新拉取仍返回旧正文。
本轮发布入口修订已提交为 `kasumi/index` `755b90a0-0a5f-41d8-aa14-92267e4afdc3`、
`kasumi/configuration` `2c97f846-97ab-4969-b143-195d640e63e3` 和
`kasumi/development` `0f3cc5d1-9b98-48cb-bb2c-08d4534290a6`，当前状态为
`pending`，尚未确认线上生效。
本轮单窗口多 View 文档修订为 `kasumi/development`
`1fc2b123-b274-4a60-bb87-b6774a158b56`，已补充共享 Environment、子 `HWND`、
独立路由与生命周期、无面积边界拒绝规则，当前状态为 `pending`。
多 WebView 示例应用修订为 `kasumi/development`
`afb06dad-c981-462d-b10f-1df2bfc59d0f`，补充 `--multi-view`
构建入口、`multi-view.html` 双实例页面、共享便签和 CTest 覆盖，当前状态同样为
`pending`。
发布状态以站点实际页面为准，未发布前不得描述为已上线。

同步流程固定为：先用 `docs-rev.py pull` 拉取原文，再用本地编辑器修改，执行
`push --dry-run` 检查 diff，确认后推送修订，最后重新拉取或读取线上原文确认内容已生效。
未确认生效的页面不得写成已上线。
