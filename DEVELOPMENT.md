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
| Windows WebView2 适配 | `windows/src/platform/ksm_webview2_host.h` | WebView2、消息来源、导航拦截、窗口事件和 UI 线程回投 |
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
- `KSMWebView2Host` 负责 WebView2 创建、虚拟主机映射、消息来源校验、导航与新窗口拦截、重载和尺寸事件。
- 每个窗口分配单调递增且不复用的 `windowId`；同一窗口复用同一个路由器和宿主服务实例。
- `windows/` 只构建 SDK；`windows/examples/win32-host/` 是独立消费者，加载 `https://app.local/index.html`。
- SDK 安装目录包含 `include/kasumi/`、`kasumi_windows_bridge.lib` 和 `lib/cmake/Kasumi/`。
- 示例依赖 Evergreen WebView2 Runtime，缺失时显示安装提示并退出；ZIP 不内置 Fixed Version Runtime。
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
| `git diff --check` | 通过；仅有仓库既有的 LF/CRLF 转换警告 |

本机未提供 CMake、MSVC 或 Gradle，当前轮次没有重新执行 Windows CMake/CTest 和 Android
Gradle/instrumentation 构建。提交前必须在 CI 或具备对应工具链的机器上完成以下检查：

1. Windows CMake + vcpkg `webview2` 构建、`kasumi_contract` 和 `kasumi_win32_example_smoke`。
2. Windows ZIP 内容检查，以及 Evergreen Runtime 缺失提示和标签签名流程。
3. Android JVM 单测、API 34 模拟器 connected tests 和真机生命周期测试。
4. Android 下载取消、通知权限、文件选择器、旋转和 `onTrimMemory` 的真实设备验证。

## 常用命令

```bash
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
python cli/ksm.py package --platform android --target sdk
python cli/ksm.py package --platform android --target example
```

Windows 构建需要 vcpkg 的 `webview2` 与 `nlohmann-json` 依赖。SDK 安装到
`windows/build/sdk/install`；示例启动前确认 `web/` 与 EXE 位于同一输出目录。

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

本轮 SDK/示例分离与共享能力页变更仍待同步到线上草稿。同步前不得把相关页面描述为已上线。

同步流程固定为：先用 `docs-rev.py pull` 拉取原文，再用本地编辑器修改，执行
`push --dry-run` 检查 diff，确认后推送修订，最后重新拉取或读取线上原文确认内容已生效。
未确认生效的页面不得写成已上线。
