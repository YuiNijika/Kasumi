# Kasumi 开发入口

> 从协议契约、前端桥接、Windows 宿主到 Android 宿主的开发顺序、当前进度、验证入口与排查路径。

## 模块边界

| 模块 | 入口 | 负责内容 |
| --- | --- | --- |
| 协议 | `protocol/commands.json` | 命令、事件、参数、返回结构、权限和平台能力 |
| 代码生成 | `tools/codegen.py` | 生成 TypeScript、C++、Kotlin 契约常量与参数校验器 |
| 前端 | `frontend/src/index.ts` | `KSM` 根入口、事件总线和 API 封装 |
| Windows 根入口 | `windows/src/commands/ksm_phase1.h` | 宿主侧 `KSM` 生命周期、统一调用和原生事件 |
| Windows 路由 | `windows/src/bridge/ksm_router.h` | 请求校验、权限检查和命令分发 |
| Windows WebView2 适配 | `windows/src/platform/ksm_webview2_host.h` | WebView2、内嵌资源、消息来源、导航拦截、窗口事件和 UI 线程回投 |
| Windows SDK 包 | `windows/CMakeLists.txt` | 静态库、公共头文件、CMake package config 和 CPack |
| Windows 示例 | `windows/examples/win32-host/` | 独立消费者工程，使用已安装 SDK |
| Android SDK | `android/kasumi/` | AAR、宿主服务、路由、生成契约和公开 API |
| Android 示例 | `examples/android-host/` | 独立消费者工程，使用本地 Maven 中的 AAR |
| Android 测试宿主 | `android/app/` | 框架维护者的 JVM 与 instrumentation 测试宿主 |
| 契约测试 | `tests/contract/run.ts` | 前端与双平台模拟宿主的共享行为测试 |
| CLI | `cli/ksm.py` | 协议校验、代码生成、双端构建、测试和打包入口 |

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
- `KSMWebView2Host` 负责 WebView2 创建、内嵌或外置资源响应、消息来源校验、导航与新窗口拦截、重载和尺寸事件。
- 每个窗口分配单调递增且不复用的 `windowId`；同一窗口复用同一个路由器和宿主服务实例。
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
| `pnpm ksm build --windows --target example` | 默认输出仅 EXE，无 `WebView2Loader.dll` 或 `.WebView2` |
| `pnpm ksm build --windows --target example --with-webview2-loader` | 可选输出包含 `WebView2Loader.dll` |
| `pnpm ksm package --windows --target example` | 默认 ZIP 仅包含 EXE |
| `pnpm ksm dev` | Windows 宿主加载 `http://localhost:5173/index.html` |
| `pnpm ksm dev --android` | 模拟器通过 `adb reverse` 加载开发页面 |
| `pnpm ksm test --android --target sdk` | 契约测试与 Android JVM 测试通过 |
| `kasumi_win32_example_smoke` | Windows 示例启动、ping 与退出通过 |
| `git diff --check` | 通过；仅有仓库既有的 LF/CRLF 转换警告 |

Windows 已完成 SDK、内嵌 Web 示例构建和真实桌面 smoke test，`kasumi_contract` 与
`kasumi_win32_example_smoke` 均通过。默认示例输出只有 EXE，没有 `WebView2Loader.dll`、
`kasumi_win32_example.exe.WebView2`、`web/` 或 `KasumiData/`。SDK ZIP 只包含公共头文件、
静态库、CMake 配置和 README；默认示例 ZIP 只包含 EXE，显式启用 Loader 打包时才额外包含
`WebView2Loader.dll`，外置模式 ZIP 额外包含 `web/`。

Android 已完成 6 个 JVM 测试、AAR 发布、维护者 APK、instrumentation 测试 APK 和独立
示例 APK 构建。AAR 和本地 Maven 发布物不包含 Kotlin 源码或 sources JAR，示例 APK 的
Web 资源位于 `assets/`。本机 JDK 的 `TEMP` 包含短文件名时，Gradle 的 AF_UNIX 连接会
报告 `Unable to establish loopback connection`，需要显式指定 socket 目录。
API 34 模拟器已完成 3 项 connected tests，并验证示例页面实际加载。Android 宿主统一使用
`KSMWebAssetLoader.create(context, assetRoot)`；该入口固定域名 `app.local`，避免
`WebViewAssetLoader` 默认域名不匹配导致 `ERR_NAME_NOT_RESOLVED`。

仍需在真机完成以下检查：

1. Android 下载取消、通知权限、文件选择器、旋转和 `onTrimMemory` 的真实设备验证。
2. Windows Evergreen Runtime 缺失提示和标签签名流程。

## pnpm 命令入口

根目录通过 `package.json` 暴露统一的 `pnpm ksm` 入口，底层仍调用 `cli/ksm.py`：

```bash
pnpm ksm build
pnpm ksm build --windows
pnpm ksm build --android
pnpm ksm build --all
pnpm ksm build --windows --with-webview2-loader
pnpm ksm dev
pnpm ksm dev --android
```

`build` 默认只构建 Windows；`--all` 同时构建 Windows 和 Android。旧的
`--platform windows|android|all` 继续可用。`dev` 默认启动 Windows 宿主，Android
开发模式会启动 `http://localhost:5173` 静态服务器、执行 `adb reverse`、安装示例 APK
并从该地址加载页面。

Windows 默认使用 vcpkg 的 `x64-windows-static-md` triplet，静态链接 WebView2 Loader，
因此输出目录不需要 `WebView2Loader.dll`，运行时使用系统已安装的 Evergreen Runtime。
需要随应用分发 Loader 时传入 `--with-webview2-loader`，该模式使用 `x64-windows`
triplet，并写入独立的 `windows/build/sdk-webview2-loader` 与
`windows/examples/win32-host/build-webview2-loader` 目录。

如果 Python 不在 `PATH`，设置 `KASUMI_PYTHON` 指向 Python 3.10+ 可执行文件。

## 常用命令

```bash
pnpm ksm build --windows --target sdk
pnpm ksm build --android --target example
pnpm ksm build --windows --target example --with-webview2-loader
pnpm ksm dev
pnpm ksm dev --android
python -m pip install -e .
python tools/validate_protocol.py
python tools/codegen.py
node frontend/node_modules/typescript/bin/tsc --noEmit -p frontend/tsconfig.json
node frontend/node_modules/typescript/bin/tsc --noEmit -p tests/tsconfig.json
node frontend/node_modules/tsx/dist/cli.mjs tests/contract/run.ts
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
发布状态以站点实际页面为准，未发布前不得描述为已上线。

同步流程固定为：先用 `docs-rev.py pull` 拉取原文，再用本地编辑器修改，执行
`push --dry-run` 检查 diff，确认后推送修订，最后重新拉取或读取线上原文确认内容已生效。
未确认生效的页面不得写成已上线。
