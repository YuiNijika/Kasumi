# Kasumi 开发入口

> 从协议契约、前端桥接、Windows 宿主到 Android 宿主的开发顺序、验证入口与排查路径。

## 模块边界

| 模块 | 入口 | 负责内容 |
| --- | --- | --- |
| 协议 | `protocol/commands.json` | 命令、事件、权限和平台能力 |
| 代码生成 | `tools/codegen.py` | 生成 TypeScript、C++、Kotlin 契约常量与校验器 |
| 前端 | `frontend/src/index.ts` | `KSM` 根入口、事件总线和 API 封装 |
| Windows 根入口 | `windows/src/commands/ksm_phase1.h` | 宿主侧 `KSM` 生命周期、统一调用和原生事件 |
| Windows | `windows/src/bridge/ksm_router.h` | WebView2 消息路由、权限和命令分发 |
| Windows 宿主 | `windows/src/platform/ksm_webview_host.h` | 线程池、超时、取消、单响应和事件投递 |
| Android 根入口 | `android/app/src/main/java/dev/kasumi/app/bridge/KSM.kt` | 宿主侧 `KSM` 生命周期、统一调用和原生事件 |
| Android | `android/app/src/main/java/dev/kasumi/app/bridge/KSMRouter.kt` | WebView 消息路由、权限和命令分发 |
| Android 宿主 | `android/app/src/main/java/dev/kasumi/app/MainActivity.kt` | WebView、协程、资源加载和系统事件 |
| 契约测试 | `tests/contract/run.ts` | 前端与双平台模拟宿主的共享行为测试 |
| CLI | `cli/ksm.py` | 协议校验、代码生成、双端构建、测试和打包入口 |
| CI | `.github/workflows/ci.yml` | Windows CMake/CTest 与 Android 模拟器契约测试 |
| 发布打包 | `.github/workflows/release.yml` | Windows ZIP 与已签名 Android APK |

## 当前进度

在线开发文档中的 Phase 1-5 桥接层任务已经完成，当前进入真实宿主集成与平台回归阶段：

- `protocol/commands.json` 定义 25 个内置命令和 2 个事件，三端使用同一份命令、权限和参数校验器。
- 前端 `KSM` 根入口提供 `init()`、`shutdown()`、`invoke()`、`on()`、缓存的 `get()` / `capabilities()`。
- Windows 与 Android 宿主侧 `KSM` 根入口提供 `Init/init`、`Shutdown/shutdown`、`Invoke/invoke`、`On/on` 和缓存的 `Get/get`。
- Windows 根入口挂载 `Fs`、`Dialog`、`Clipboard`、`Notification`、`Permission`、`Window`、`App`；Android 根入口挂载 `fs`、`dialog`、`clipboard`、`notification`、`permission`、`window`、`app`。
- Windows `KSM::Invoke(windowId, ...)` 按 `windowId` 维护独立路由器，`windowId` 不再只是请求中的无效字段。
- 宿主根入口直接调用 `KSMHostServices`，不通过 `KSM::Invoke` / `KSM.invoke` 递归回路由。Android 只暴露平台契约支持的 `window.close`，不伪造桌面窗口能力。
- Windows `KSM::Fs` 与 Android `KSMFs` 均覆盖可写沙箱、只读 `app://res` / `app://assets` 和 `app://picked/<id>`，与路由命令保持同一能力边界。
- CLI 提供 `codegen`、`validate`、`build`、`test`、`package`、`dev`，其中构建、测试和打包可用 `--platform windows|android|all` 选择目标。
- Windows 使用 CPack 生成 `kasumi-windows-bridge-0.1.0.zip`；Android release 从环境变量读取 keystore 与口令，缺少签名材料时只保留未签名本地构建。
- CI 在 Windows 执行 CMake/CTest，在 Android 执行 JVM 单测、instrumentation 测试编译和 API 34 模拟器契约测试。
- `fs.readText` 返回 `string` 或 `{ file }`。超过 64KiB 的结果会写入 `app://tmp/read-<id>.txt`，再次读取该临时句柄会返回同一句柄。
- `app://data`、`app://cache`、`app://tmp` 可读写；`app://picked/<id>` 表示用户选择的文件；`app://res` 与 `app://assets` 只读。
- `fs.readDir` 返回按名称排序的目录项；`fs.mkdir` 只在可写沙箱创建目录；`fs.stat` 支持可写沙箱、只读资源和有效 picked 句柄。`modifiedAt` 使用 Unix 毫秒或 `null`，目录 `size` 为 `0`。
- 前端和双端原生路径守卫都会拒绝空字节、`..`、`~`、根路径、盘符绝对路径、盘符相对路径、NTFS 流、反斜杠、未注册虚拟根和越权虚拟根。
- 单条 JSON 消息限制为 64KB，单文件读写限制为 16MB。
- `webview.reloaded` 是系统事件，不依赖 `event.subscribe`；`window.resized` 需要正常订阅。
- 前端传输层同时监听注入对象和同窗口 `message` 事件，Android 通过 `window.postMessage` 发送的系统和路由事件可以进入同一总线。

Windows 与 Android 的桥接层已经实现：

- Windows `KSMWebViewHost` 使用 8 个固定工作线程和最多 32 条排队任务，支持 `timeoutMs`、`cancel`、`BUSY` 与单响应保护；销毁时会标记未完成请求并丢弃迟到响应。
- Windows CMake 目标启用 `UNICODE` / `_UNICODE`，`vcpkg.json` 固定 `builtin-baseline`，避免宽字符资源宏与新版 vcpkg 清单解析导致构建失败。
- Android 使用 `WebViewCompat.addWebMessageListener`、最多 8 条并发命令、`withTimeout`、`runInterruptible` 和主线程回发。
- Windows 消息来源同时接受配置 origin 的精确值和带路径的完整文档 URL，并拒绝 `https://app.local.evil.example` 一类近似域名。
- Android WebView 启用 CSP、导航拦截、`window.open` 拦截，生产环境关闭调试，不使用 `addJavascriptInterface`；外部导航只允许 `http` 和 `https` scheme。
- Android 13 及以上的通知权限通过 `permission.request` 和 `POST_NOTIFICATIONS` 处理。

### 已执行验证

| 检查 | 当前结果 |
| --- | --- |
| `python tools/validate_protocol.py` | 通过 |
| `python tools/codegen.py` | 通过 |
| 前端 TypeScript 类型检查 | 通过 |
| 测试工程 TypeScript 类型检查 | 通过 |
| `tests/contract/run.ts` | `frontend contract passed for windows and android`，覆盖事件来源隔离、文件读写、目录读取、目录创建、元数据、对话框、剪贴板、通知、权限、窗口和应用命令 |
| Windows 可移植 C++ 源码 NDK 语法检查 | 通过 |
| Windows MSVC 源码编译与契约 runner | 通过，输出 `windows contract passed` |
| Windows CMake/Ninja 构建与 CTest | 通过，`kasumi_contract` 1/1 tests passed |
| Windows CPack ZIP | 通过，包含公共头文件与 `kasumi_windows_bridge.lib` |
| Android 主源码 Kotlin 编译 | 通过；`KSMAndroidHostServices` 与 bridge/generated 源码一起编译通过，仅保留 `Notification.Builder(Context)` 弃用警告 |
| Android `ContractTest` 与 `NavigationPolicyTest` JUnit | `OK (3 tests)`，覆盖根入口全部子模块、只读与 picked 路径、关闭后不可用、来源与外部导航白名单 |
| Android `BridgeInstrumentedTest` 静态编译 | 通过，覆盖路径守卫、大文本句柄、事件分发、只读与 picked 根入口能力 |
| `git diff --check` | 通过；仅输出仓库既有的 CRLF 转换警告 |
| CLI `--help` 与子命令解析 | 通过 |

### 仍需真实宿主验证

- Windows 桥接库已通过 MSVC、CMake、Ninja 和 CTest；仍需在真实 WebView2 窗口生命周期中完成集成运行验证。
- 实际 WebView2 适配器必须接入 `KSMWebViewHost::NotifyWebViewReloaded`、`NotifyWindowResized`、导航拦截、`window.open` 拦截、CSP 和消息来源校验。
- Windows 多窗口路由已落在 `KSM::Invoke(windowId, ...)`，真实应用壳仍需保证每个 WebView2 使用稳定且唯一的 `windowId`。
- Android 仍需在真机或模拟器上验证 `withTimeout + runInterruptible`、资源加载器、运行时权限和 `onTrimMemory`。
- Windows 阻塞式对话框无法被线程取消真正终止；当前策略是先返回超时或取消，再抑制迟到响应。
- 宿主根入口的剪贴板、通知和窗口方法已接入平台服务，但仍需在真实 Windows 窗口与 Android 真机上验证系统权限、通知栏和生命周期行为。
- 本机 vcpkg registry 拉取在 Windows 文件系统上报 `Improper link`，因此 CMake 验证使用了本地 header-only `nlohmann/json` 包配置；GitHub Actions 的 vcpkg 清单安装仍应以 CI 结果为准。
- 本机 Gradle 因运行环境无法建立 JVM 回环连接，未完成 Gradle DSL、instrumentation test 编译和模拟器测试的本地执行；这些检查已写入 CI，仍需以 CI 结果为准。

## 开发顺序

1. 修改 `protocol/commands.json`、`protocol/commands.schema.json` 和 `protocol/permissions.schema.json`。
2. 运行 `python tools/validate_protocol.py` 和 `python tools/codegen.py`。
3. 实现 Windows `KSMRouter` 与 Android `KSMRouter` 的命令分支。
4. 在 `frontend/src/api/` 增加前端封装，并挂到 `frontend/src/bridge/ksm.ts` 的 `KSM` 根入口。
5. 在 `tests/contract/cases/` 增加共享用例，并同步 `windows/tests/contract_runner.cpp` 与 `android/app/src/test/.../ContractTest.kt`。
6. 执行类型检查、前端契约测试和对应平台构建。

## 常用命令

```bash
python -m pip install -e .
ksm --help
python tools/validate_protocol.py
python tools/codegen.py
npx --yes -p typescript@5.6.3 tsc --noEmit -p frontend/tsconfig.json
npx --yes -p typescript@5.6.3 tsc --noEmit -p tests/tsconfig.json
npx --yes -p tsx@4.19.2 tsx tests/contract/run.ts
python cli/ksm.py build --platform all
python cli/ksm.py test --platform all
python cli/ksm.py package --platform android
```

CI 的 Windows 与 Android job 都会先执行前端契约测试，再分别运行 CMake/CTest 和 Android
JVM 单测、instrumentation 测试编译与 API 34 模拟器测试。标签发布工作流会生成 Windows ZIP
和已签名 Android APK。

## 安全边界

- 前端只使用 `app://` 虚拟路径，不直接访问真实文件路径。
- 文件选择器返回 `app://picked/<id>`，不向前端暴露真实路径或 `content://` URI。
- 路径必须先拒绝危险输入，再进行前缀解析；不得先规范化再校验。
- 业务代码不得直接调用 `window.chrome.webview`、`window.ksm` 或 `WebViewCompat`，必须经过 `KSM`。
- 单条 JSON 消息不能超过 64KB，单文件读写不能超过 16MB。
- Android 不得使用 `addJavascriptInterface`，必须保持 CSP、导航拦截和 `window.open` 拦截。
- Windows 生产环境必须关闭 DevTools，并校验 WebView2 消息来源。

## 排查路径

1. 确认命令已经写入 `protocol/commands.json`，并运行协议校验和代码生成。
2. 确认生成文件包含命令、权限、事件和参数校验器。
3. 确认 `capabilities.query` 返回平台实际支持的命令。
4. 确认请求经过参数校验、权限校验和路径守卫。
5. 确认响应 `id` 与请求一致，并且每个请求最多收到一个响应。
6. 检查超时与取消是否由宿主线程池或协程实现，而不是只在前端丢弃 Promise。
7. 检查 WebView 回调是否切回 UI 线程或主线程。
8. 检查事件是否由注入对象或同窗口 `window.postMessage` 到达；跨窗口来源会被前端传输层拒绝。
9. 检查 Windows `windowId` 是否稳定；同一窗口必须复用同一个路由器。
