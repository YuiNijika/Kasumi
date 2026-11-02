# Kasumi 开发入口

> 从协议契约、前端桥接、Windows 宿主到 Android 宿主的开发顺序、当前进度、验证入口与排查路径。

## 模块边界

| 模块 | 入口 | 负责内容 |
| --- | --- | --- |
| 应用配置 | `kasumi.config.json` | 两端包名、版本、命令 allowlist 和 Windows 标题栏开关 |
| 协议 | `protocol/commands.json` | 命令、事件、参数、返回结构、权限和平台能力 |
| 插件清单 | `protocol/plugins.json` | 内置插件归属、版本和命令所有权 |
| 版本元数据 | `version.json` | 框架产品版本、Release 标签和所有构建产物的版本来源 |
| 代码生成 | `tools/codegen.py` | 生成 TypeScript、C++、Kotlin 契约常量与参数校验器 |
| 前端 | `frontend/src/index.ts` | `KSM` 根入口、事件总线和 API 封装 |
| Windows 根入口 | `windows/src/commands/ksm_phase1.h` | 宿主侧 `KSM` 生命周期、统一调用和原生事件 |
| Windows 路由 | `windows/src/bridge/ksm_router.h` | 请求校验、权限检查和命令分发 |
| Windows 插件运行时 | `windows/src/plugins/ksm_plugin.h` | 插件 ID、命令归属、参数校验和宿主上下文 |
| Windows 内置插件 | `windows/src/plugins/ksm_*_plugin.*` | 文件、对话框、剪贴板、通知、窗口、网络、SQL、路径、存储、系统和日志 |
| Windows SQL 驱动 | `windows/src/platform/ksm_sql_driver.h` | 连接注册表、驱动 SPI 和 SQLite 内置实现 |
| Windows 远程 SQL | `windows/src/platform/ksm_odbc_sql_driver.h` | 系统 ODBC 发现、MySQL / PostgreSQL 参数化连接 |
| Windows 窗口框架 | `windows/src/platform/ksm_window_frame.h` | 原生标题栏隐藏、缩放边框和工作区处理 |
| Windows WebView2 适配 | `windows/src/platform/ksm_webview2_host.h` | 共享 Environment、单窗口多 View、内嵌资源、消息隔离和 UI 线程回投 |
| Windows SDK 包 | `windows/CMakeLists.txt` | 静态库、公共头文件、CMake package config 和 CPack |
| Windows 示例 | `windows/examples/win32-host/` | 独立消费者工程，使用已安装 SDK |
| Android SDK | `android/kasumi/` | AAR、宿主服务、路由、生成契约和公开 API |
| Android 插件运行时 | `android/kasumi/src/main/java/dev/kasumi/app/bridge/KSMPlugin.kt` | 插件注册表、默认插件和自定义插件入口 |
| Android 内置插件 | `android/kasumi/src/main/java/dev/kasumi/app/bridge/KSM*Plugin.kt` | 与 Windows 对齐的命令插件和宿主服务适配 |
| Android SQL 驱动 | `android/kasumi/src/main/java/dev/kasumi/app/bridge/KSMSqlDriver.kt` | 驱动 SPI、连接生命周期和 SQLite 内置实现 |
| Android WebView 适配 | `android/kasumi/src/main/java/dev/kasumi/app/bridge/KSMWebViewHost.kt` | 消息桥、命令并发、导航、生命周期和资源释放 |
| Android WebView 运行时 | `android/kasumi/src/main/java/dev/kasumi/app/bridge/KSMWebViewRuntime.kt` | 安全默认设置、沉浸式全屏、前后台暂停和低内存回收 |
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

当前协议基线为 **48 个命令、2 个事件、14 个内置插件**。产品版本只维护在根目录
`version.json`。SQL 插件新增命令使用 `since: "1.2.0"`，`path`、`store`、`os` 和
`log` 使用 `since: "1.3.0"`，命令名和参数以
`protocol/commands.json` 为准。

版本维护入口：

| 命令 | 作用 |
| --- | --- |
| `pnpm version:show` | 输出 `version.json` 中的当前版本 |
| `pnpm version:sync` | 同步 npm 包、应用配置、vcpkg manifest 和 `pyproject.toml` |
| `pnpm version:check` | 检查所有必须保持同步的版本字段 |

Windows CMake 从 `version.json` 生成 `ksm_version.h`，Android Gradle 生成
`KSMVersionMetadata.kt`，前端契约模拟宿主直接读取 JSON。`package.json`、应用配置、
`windows/vcpkg.json` 和 `pyproject.toml` 仍需保留版本字面量，由 `version:sync`
统一更新；不要在构建脚本、运行时源码或测试夹具中重新写死版本。

| 平台 | 适用命令 | 已实现 | 说明 |
| --- | ---: | ---: | --- |
| Windows | 47 | 47 | 不含仅 Android 提供的 `device.getInfo` |
| Android | 44 | 44 | 不含 Windows 专属的窗口标题、最小化、最大化、尺寸调整 |
| 协议全集 | 48 | 48 | 两端按平台矩阵过滤，不伪造跨平台能力 |

本轮新增：

- `http.download`：HTTPS 下载到可写虚拟路径，支持取消、120 秒总超时和 100 MiB 上限。
- `shell.openExternal`：只允许宿主显式配置的精确 origin，使用系统浏览器打开。
- `device.getInfo`：仅 Android 提供非持久设备、显示和运行时信息。
- `window.setSize`：仅 Windows 提供逻辑外框尺寸调整，最小 `320 × 240`。
- `sql.drivers`、`sql.open`、`sql.query`、`sql.execute`、`sql.close`：统一 SQL 插件命令，
  两端内置 SQLite；Windows 在检测到系统 ODBC 驱动时自动提供 MySQL / PostgreSQL，
  其余远程驱动通过宿主 `KSMSqlDriver` SPI 注册。
- `path.*`：在不接触真实文件系统的前提下检查、拼接和规范化虚拟路径。
- `store.*`：用应用数据目录内的 JSON 键值存储保存小型结构化状态。
- `os.getInfo`：返回不含设备标识、主机名和用户信息的系统与运行时信息。
- `log.write`：写入平台调试日志，限制消息和 target 大小，不持久化、不上传。
- `plugins.json`：为全部协议命令建立插件归属，生成前端、C++ 和 Kotlin 插件常量。
- `fs`、`dialog`、`clipboard`、`notification`、`window`、`http`、`shell`、`device`、
  `sql`、`path`、`store`、`os` 和 `log` 已抽成 Windows / Android 原生插件，保留原有
  命令和根 API 兼容性。

前端 `KSM` 根入口已挂载 `http`、`shell`、`device`、`path`、`store`、`os`、`log`
和 `window.setSize`。Windows 与 Android 宿主侧插件通过统一路由派发；Android 不提供
Windows 专属窗口方法。`KSM.sql` 在两端都可用，实际可用驱动由 `sql.drivers` 返回。

发布边界已完成：

- Windows SDK 产出 `kasumi-windows-sdk-<version>.zip`，示例单独产出可执行程序 ZIP。
- Android SDK 产出 `kasumi-android-sdk-<version>.aar`，示例单独产出 APK。
- `windows/examples/win32-host/` 通过 `find_package(Kasumi CONFIG REQUIRED)` 消费 SDK。
- `examples/android-host/` 通过 `dev.kasumi:kasumi:<version>` 消费本地 Maven 中的 AAR。
- Windows SDK 包不包含 `.cpp`，Android Maven 发布不生成 sources JAR。

当前发布加固边界：

- 用户项目 `ksm package` 输出到 `.ksm/dist/packages/`；框架附件输出到 `.ksm/release/kasumi/<version>/`，示例应用输出到 `.ksm/release/app/<version>/`。
- Android release 缺少四个签名变量时在构建前失败，APK 通过 `apksigner verify` 后才复制到发布目录。
- `ksm doctor` 使用框架识别逻辑检查 Next.js 与 Nuxt，不再硬编码根级 `frontend/index.html`。
- SDK 下载只允许 HTTPS，测试额外允许回环 HTTP；manifest、资产大小、超时、重定向和 ZIP 路径均有上限。
- SDK 先下载并解压到项目内 `.tmp/`，与目标目录保持同卷；双平台全部校验后再统一提交，任一提交失败会回滚已安装的 SDK 并保留旧版本。
- 初始化失败会删除本次新建的项目目录，不会留下半安装 SDK。
- POSIX 开发服务器按进程组终止，Windows 使用 `taskkill /T /F` 终止子进程树。
- 旧维护者 CLI 尊重 `GRADLE`，并在 Android 工程目录执行系统 Gradle。
- 生产 WebView 只接受 `https://app.local`；开发 origin 只在 Windows dev 或 Android debug 中开放。
- Android WebView 外部导航直接拦截，统一要求使用 `shell.openExternal` 的精确 origin 白名单。
- `basic` Android 模板复用 `KSMWebViewHost`，不再手写外部导航或调用 `ACTION_VIEW`。
- `kasumi-bridge` 输出使用显式 `.js` 相对导入，安装后的 ESM 包可由 Node 直接加载。
- 根锁文件只包含根工程和 `packages/*`，CI 使用 `--frozen-lockfile` 并检查生成契约漂移。

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

1. 修改 `protocol/commands.json`、`protocol/commands.schema.json`、
   `protocol/plugins.json` 和 `protocol/permissions.schema.json`。
2. 运行 `python tools/validate_protocol.py`。
3. 运行 `python tools/codegen.py`，同步 `frontend/src/generated/contract.ts`、`windows/src/generated/contract.h` 和 `android/kasumi/src/main/java/dev/kasumi/app/generated/KSMGeneratedContract.kt`。
4. 实现 Windows `KSMRouter`、Android `KSMRouter` 和宿主服务。
5. 增加前端 API、契约测试和平台测试。

`capabilities.query` 返回宿主实际支持的 `commands` 和已授予的 `permissions`。前端调用
`KSM.get()` 读取缓存能力；页面重载后 `KSM` 会清理缓存并重新查询。请求 ID 使用每次文档
会话随机生成的前缀，避免页面重载后的计数器与新文档旧请求碰撞。`timeoutMs` 必须是
`1..600000` 范围内的整数，页面重载时前端发送 `cancel` 并拒绝未完成 Promise，宿主同步
取消旧任务和计时器。

## 插件系统

插件清单位于 `protocol/plugins.json`，每个协议命令必须且只能归属一个插件。清单同时
记录 `frontendPackage`、平台矩阵、权限、默认权限和命令所有权。`codegen` 生成
`KSM_PLUGINS`、`KSM_PLUGIN_METADATA`、`KSM_PLUGIN_COMMANDS`、
`KSM_COMMAND_PLUGINS` 以及 C++、Kotlin 的同源元数据；运行时插件注册表只接受唯一 ID
和唯一命令，并把插件权限合并到能力缓存。

| 层级 | 前端 | Windows | Android |
| --- | --- | --- | --- |
| 清单 | `KSM_PLUGINS` | `KSMGeneratedContract::Plugins()` | `KSMGeneratedContract.plugins` |
| 元数据 | `KSM_PLUGIN_METADATA` | `KSMGeneratedContract::PluginMetadata()` | `KSMGeneratedContract.pluginMetadata` |
| 命令入口 | `KSM.invoke` | `KSMPlugin::Handle` | `KSMPlugin.handle` |
| 参数校验 | 生成的 schema | `KSMPlugin::ValidateArgs` | `KSMPlugin.validateArgs` |
| 宿主上下文 | 无 | `KSMPluginContext` | `KSMPluginContext` |

除 `core` 外，插件清单中的模块都由原生插件运行时注册，并各自发布为
`kasumi-plugin-<id>` 前端包。插件包只重新导出 `kasumi-bridge` 的模块入口，不复制原生
实现。自定义插件通过 `KSMWebView2Host` / `KSMWebViewHost` 的 `plugins` 参数注册；除
`core` 外的生成插件 ID 都允许用同 ID 插件显式替换，但必须完整声明该 ID 的全部生成命令。
自定义插件不得抢占其他生成命令。自定义插件命令不在 `commands.json` 时，必须由插件
自己校验参数并返回协议错误码。

`core` 仍由路由层承载，因为应用生命周期、权限和事件需要访问路由器内部状态。`device`
只在 Android 注册，Windows 对 `device.getInfo` 返回 `NOT_SUPPORTED`。`path`、`store`、
`os` 和 `log` 在两端注册，其中 `store` 固定写入
`app://data/.kasumi/store.json`，`os.getInfo` 不采集持久设备标识。插件上下文包含当前
请求 ID，下载插件把它传给宿主，确保取消消息可以中止活动网络请求。

Windows 插件实现入口为 `windows/src/plugins/ksm_plugin.h`；Android 入口为
`android/kasumi/src/main/java/dev/kasumi/app/bridge/KSMPlugin.kt`。插件不得直接读取
`window.chrome.webview`、`addJavascriptInterface` 或绕过 `KSMRouter`。

Windows 公共头文件同时提供 `KSM::Path`、`KSM::Store`、`KSM::Os` 和 `KSM::Log`
类型化入口，分别覆盖虚拟路径、JSON 键值存储、系统信息和固定级别日志。它们复用插件
路由与权限校验，只能在插件处理器之外调用，避免命令处理期间递归进入路由。

## SQL 插件

`KSM.sql` 暴露 `drivers`、`open`、`query`、`execute` 和 `close`。SQLite 在两端内置。
Windows 启动时发现系统 MySQL / MariaDB 和 PostgreSQL ODBC 驱动并注册对应标识；驱动
未安装时不会出现在 `sql.drivers`。Android、MongoDB 和其他自定义数据库使用同一
`KSMSqlDriver` SPI，由宿主注册适配器后才会出现在 `sql.drivers`。协议中的驱动标识
不等于客户端已经随应用打包。

```ts
const available = await KSM.sql.drivers();
if (!available.drivers.includes('sqlite')) {
  throw new Error('SQLite driver missing');
}

const connection = await KSM.sql.open({
  driver: 'sqlite',
  url: ':memory:'
});

await KSM.sql.execute({
  connectionId: connection.connectionId,
  statement: 'CREATE TABLE notes (id INTEGER PRIMARY KEY, body TEXT NOT NULL)'
});

await KSM.sql.execute({
  connectionId: connection.connectionId,
  statement: 'INSERT INTO notes (body) VALUES (?)',
  params: ['hello']
});

const rows = await KSM.sql.query({
  connectionId: connection.connectionId,
  statement: 'SELECT id, body FROM notes ORDER BY id'
});

await KSM.sql.close({ connectionId: connection.connectionId });
```

SQLite 只接受 `:memory:` 或 `app://data`、`app://cache`、`app://tmp` 下的可写文件路径。
远程驱动的 URL、TLS、占位符和迁移策略由各驱动实现定义，前端不能假设四种数据库共享
同一套 SQL 方言。

每个宿主服务最多同时打开 32 个 SQL 连接，每条查询最多返回 10,000 行和 16 MiB 结果数据。
任一超限返回 `RESOURCE_LIMIT`，不截断、不静默丢弃行。Windows ODBC 和 Android SQLite
都在读取阶段执行同一上限，关闭连接后额度立即释放。

`examples/showcase` 的数据库模块会打开 `app://data/showcase/showcase.sqlite`，创建示例表，
通过参数写入记录，读取最近八条结果，并在 `finally` 中关闭连接。Windows 单 View smoke
还会用 `:memory:` 完成同样的驱动、执行、查询和关闭链路。

同一页面的基础插件模块会把 `path.dirs` 和路径运算结果、`store` 中的结构化设置、
`os.getInfo` 的系统环境以及 `log.write` 的诊断结果集中显示。清空 store 前必须调用
`dialog.confirm`，页面不会把真实用户目录传给这些插件。

Windows 驱动实现 `KSMSqlDriver` / `KSMSqlConnection`，并通过
`KSMHostServices::RegisterSqlDriver` 注册。Android 实现 `KSMSqlDriver` /
`KSMSqlConnection`，并通过 `KSMAndroidHostServices.registerSqlDriver` 注册。驱动错误
应转换为 `INVALID_ARGS`、`PERMISSION_DENIED`、`NOT_FOUND`、`IO_ERROR` 或
`NOT_SUPPORTED`，不得把连接字符串、凭据或底层异常原样暴露给前端。

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
- 同一 View 的活动请求 ID 必须唯一；重复 ID 在进入队列前返回 `BUSY`。新文档开始或
  View 关闭时静默取消旧请求、清理计时器并抑制迟到响应。
- 每个下载请求 ID 在创建 `.part` 文件前原子占用；重复下载返回 `BUSY`，取消后释放。
- Store 的完整读改写由进程级锁串行化，避免同窗口多个 View 覆盖 `store.json`。
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
- 本地 Maven 仓库位于 `android/kasumi/build/repo`，示例通过 `dev.kasumi:kasumi:<version>` 解析。
- `examples/android-host/` 是独立消费者工程，不通过 Gradle 源码替换依赖框架模块。
- `KSMAndroidHostServices` 使用 `HttpsURLConnection` 下载、`ACTION_VIEW` 打开外部链接，并从 `Build`、窗口度量、资源和时区读取设备信息。
- `KSMWebViewHost` 统一配置 WebView、消息监听、导航、IO 命令协程和 Activity 生命周期，示例不再复制桥接代码。
- 命令最多 8 条并发；每个活动请求只保留一个取消句柄，超时和取消会中断任务，结束后立即移除请求状态。
- `KSM` 使用按 `windowId` 隔离的窗口注册表；`KSM.invoke(windowId, ...)` 和
  `KSM.on(windowId, ...)` 只访问目标窗口。无窗口参数 API 仅在恰好注册一个窗口时可用，
  多窗口场景必须显式传 ID。
- `KSMWebViewHost.close()` 只注销当前窗口、取消当前窗口请求并销毁当前 WebView，不关闭
  注入的 `KSMAndroidHostServices`。Activity 或 Application 所有者统一调用
  `hostServices.shutdown()`。
- `KSMWebViewRuntime` 在后台暂停 WebView 定时器，低内存时清理内存缓存并调用 `freeMemory`，不删除磁盘缓存。
- 定时器暂停和恢复使用进程级租约计数，单个 WebView 的生命周期不会恢复其他窗口已暂停的
  WebView。
- Android O 及以上允许系统在 WebView 不可见时 waive 渲染进程优先级，降低后台内存驻留。
- `KSMWebViewHost` 默认全屏，移除 ActionBar 并隐藏状态栏、导航栏；Activity 使用 `Theme.Kasumi.Fullscreen`，焦点恢复后重新进入沉浸模式。
- `onTrimMemory` 只取消当前 `windowId` 的活动下载；`onDestroy` 会移除消息监听和布局监听、
  释放 picked URI 映射、关闭宿主服务下载并抑制迟到响应。
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
| Android WebView 全屏与内存优化 | SDK、维护者宿主、instrumentation 测试源码和独立示例 Kotlin 编译通过，JVM 测试通过 |
| `pnpm test` | 协议、TypeScript、契约、Python、Windows CTest 与 smoke test、Android release/JVM/instrumentation 编译全部通过 |
| `pnpm test:packages` | 53 个包测试通过，覆盖初始化、框架模板、SDK 双平台事务安装与回滚、ZIP 打包、Doctor、Gradle Wrapper、协议命令矩阵、插件入口和 `decorations` 配置 |
| `pnpm build:bridge` | `kasumi-bridge` 生成 ESM 与 TypeScript 声明 |
| React / Vue 模板构建 | TypeScript 检查与 Vite 生产构建通过，输出到 `frontend/dist/` |
| Next.js 模板构建 | App Router 静态导出通过，CLI 将 `frontend/out/` 同步到 `frontend/dist/` |
| Nuxt 模板构建 | 静态生成通过，CLI 将 `frontend/.output/public/` 同步到 `frontend/dist/` |
| `pnpm --filter create-kasumi pack` | npm tarball 包含模板、Gradle Wrapper、vcpkg manifest、命令数据和 bin 入口 |
| `npm exec --package create-kasumi-<version>.tgz` | 从实际 npm tarball 初始化项目通过 |
| `pnpm test:tarballs` | 三个实际 tarball 安装、三个 bin 入口和 basic、React、Vue、Next.js、Nuxt 前端构建全部通过 |
| 生成的 basic Android 模板 | 挂载本地 Maven AAR 后 `:app:compileDebugKotlin` 通过，确认共享宿主与 debug origin 门禁可编译 |
| 生成项目 `pnpm typecheck` | 模板 TypeScript 配置通过 |
| 生成项目 `vite build` | 前端构建输出到 `frontend/dist/` 通过 |
| 本地 Release HTTP 端到端 | 下载 manifest、校验 SHA-256、解压 Windows SDK 与 Android Maven 仓库通过 |
| Windows SDK ZIP 布局 | CPack 输出扁平目录，兼容旧版单顶层目录自动展开 |
| 生成项目 Windows 构建 | `pnpm ksm build --windows` 产出 EXE，vcpkg 按 manifest 自动安装依赖 |
| 生成项目 Windows smoke test | WebView2 加载、前端 `ping`、`app.exit` 后 2.3 秒内退出 |
| 生成项目 Android 构建 | JDK 21 + 自带 Gradle Wrapper 产出 debug APK |
| Windows 批处理入口 | `pnpm.cmd` 与 `gradlew.bat` 均通过 `cmd.exe` 启动，不再触发 `spawn EINVAL` |
| `pnpm ksm package --android --target sdk` | AAR 与 Maven ZIP 汇总到 `.ksm/release/kasumi/<version>/` |
| Android Maven 归档 | 发布前清空本地 Maven 仓库，ZIP 只包含当前版本目录与元数据 |
| `pnpm ksm build --all --target example` | Windows EXE 与 Android APK 直接输出到 `.ksm/release/app/<version>/` |
| `pnpm release:prepare` | 框架与示例附件分别生成到 `.ksm/release/kasumi/`、`.ksm/release/app/`，manifest 文件大小和 SHA-256 校验通过 |
| `kasumi_win32_example_smoke` | Windows 示例启动、ping 与退出通过 |
| `kasumi_win32_example_multi_view_smoke` | 实际加载 `multi-view.html` 两侧页面，验证同窗口双 View、独立 windowId/子窗口/边界和命令路由、顶层窗口缩放重排、单 View 关闭后剩余实例继续运行 |
| 多 View 边界回归 | 创建零面积 View 被拒绝；对已存在 View 调用无面积 `SetBounds()` 失败且布局不变 |
| Windows 标题栏构建 | `ON` 与 `OFF` 均编译并通过 smoke test；`OFF` 移除 `WS_CAPTION`，客户区覆盖完整窗口，四边与四角返回对应缩放命中值 |
| Windows SDK CTest | 标题栏 `ON` 与 `OFF` 构建目录均 2/2 通过，含单 View 和多 View smoke |
| Windows 多 View 稳定性 | 多 View smoke 连续 20 次通过，并为失败状态补充具体条件诊断 |
| 维护者 Windows 发布打包 | `pnpm ksm package --windows` 将 SDK ZIP 写入 `.ksm/release/kasumi/<version>/`，示例 ZIP 写入 `.ksm/release/app/<version>/` |
| Windows 原生契约 | `kasumi_contract_runner.exe` 输出 `windows contract passed` |
| `node --test packages/*/tests/*.test.mjs` | 53/53 通过 |
| `python -m unittest tests.test_version_metadata tests.test_kasumi_config` | 19/19 通过 |
| `node cli/ksm.mjs version check` | `version.json`、npm 包、应用配置、vcpkg 和 Python 元数据一致 |
| Windows CMake 生成头 | `ksm_version.h` 与安装后的 `KasumiConfig.cmake` 均导出当前版本 |
| Android 生成源码 | `KSMVersionMetadata.kt` 与 debug APK 的 `versionName` 均来自 `version.json` |
| TypeScript 双工程检查 | `frontend/tsconfig.json` 与 `tests/tsconfig.json` 均通过 |
| `git diff --check` | 通过；仅有仓库既有的 LF/CRLF 转换警告 |

Windows 已完成 SDK、内嵌 Web 示例构建和真实桌面 smoke test，`kasumi_contract`、
`kasumi_win32_example_smoke` 与 `kasumi_win32_example_multi_view_smoke` 均通过。
同一顶层窗口可创建多个共享 Environment、但路由和生命周期互相独立的 WebView View。
默认示例输出只有 EXE，没有 `WebView2Loader.dll`、
`kasumi_win32_example.exe.WebView2`、`web/` 或 `KasumiData/`。SDK ZIP 只包含公共头文件、
静态库、CMake 配置和 README；默认示例 ZIP 只包含 EXE，显式启用 Loader 打包时才额外包含
`WebView2Loader.dll`，外置模式 ZIP 额外包含 `web/`。

Android 已完成 8 个 JVM 测试、WebView 运行时 instrumentation 测试源码、AAR 发布、
维护者 APK、instrumentation 测试 APK 和独立
示例 APK 构建。AAR 和本地 Maven 发布物不包含 Kotlin 源码或 sources JAR，示例 APK 的
Web 资源位于 `assets/`。本机 JDK 的 `TEMP` 包含短文件名时，Gradle 的 AF_UNIX 连接会
报告 `Unable to establish loopback connection`，需要显式指定 socket 目录。发布版
`kasumi-cli` 会把项目内 `.ksm/afunix` 解析为绝对路径后写入
`jdk.net.unixdomain.tmpdir`，并优先使用模板自带的 Gradle 8.14.3 Wrapper。
API 34 模拟器已完成 3 项 connected tests，并验证示例页面实际加载。Android 宿主统一使用
`KSMWebAssetLoader.create(context, assetRoot)`；该入口固定域名 `app.local`，避免
`WebViewAssetLoader` 默认域名不匹配导致 `ERR_NAME_NOT_RESOLVED`。

仍需在真机完成以下检查：

1. Android 下载取消、通知权限、文件选择器、旋转、前后台定时器和 `onTrimMemory` 的真实设备验证。
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
pnpm create kasumi my-app --template react
pnpm create kasumi my-app --template vue
pnpm create kasumi my-app --template nextjs
pnpm create kasumi my-app --template nuxtjs
```

交互式创建先询问应用目录或名称，再显示模板菜单。输入序号或模板名均可，默认
`basic`；传入 `--template <name>` 会跳过模板选择，`--yes` 则完全使用默认值。

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

| 模板 | 前端栈 | 构建方式 |
| --- | --- | --- |
| `basic` | TypeScript + Vite | Vite |
| `react` | React + TypeScript | Vite |
| `vue` | Vue + TypeScript | Vite |
| `nextjs` | Next.js App Router | 静态导出 |
| `nuxtjs` | Nuxt | 静态生成 |

`next` 与 `nuxt` 分别是 `nextjs`、`nuxtjs` 的别名。四个框架模板以基础模板为共享层，
只覆盖 `frontend/` 和根 `package.json`。框架项目及 `package.json` 由官方脚手架初始化，
Kasumi 依赖和命令通过包管理器写入，不在初始化器源码中维护手写框架依赖清单。

初始化器从 GitHub Release 下载两个 SDK 资产，并校验 Release manifest 中的 SHA-256：

| 资产 | 用途 |
| --- | --- |
| `kasumi-windows-sdk-<version>.zip` | Windows CMake package、公共头文件和静态库 |
| `kasumi-android-maven-<version>.zip` | Android AAR 及其 POM/module 元数据 |
| `kasumi-release.json` | 版本、文件名、大小和 SHA-256 |

Windows SDK ZIP 使用扁平布局，解压后直接得到 `include/`、`lib/` 和 `README.md`。
初始化器仍兼容旧版带单个顶层目录的 ZIP，避免历史 Release 无法安装。

用户项目默认安装桥接包；使用独立插件入口时再安装对应插件包：

| npm 包 | 作用 |
| --- | --- |
| `kasumi-bridge` | 前端 `KSM` API 与事件总线 |
| `kasumi-plugin-fs` | 文件系统插件前端入口 |
| `kasumi-plugin-dialog` | 对话框插件前端入口 |
| `kasumi-plugin-clipboard` | 剪贴板插件前端入口 |
| `kasumi-plugin-notification` | 通知插件前端入口 |
| `kasumi-plugin-window` | 窗口插件前端入口 |
| `kasumi-plugin-http` | HTTPS 下载插件前端入口 |
| `kasumi-plugin-shell` | 外部链接插件前端入口 |
| `kasumi-plugin-device` | Android 设备信息插件前端入口 |
| `kasumi-plugin-sql` | SQL 插件前端入口 |
| `kasumi-plugin-path` | 虚拟路径插件前端入口 |
| `kasumi-plugin-store` | JSON 键值存储插件前端入口 |
| `kasumi-plugin-os` | 系统信息插件前端入口 |
| `kasumi-plugin-log` | 平台日志插件前端入口 |
| `kasumi-cli` | `ksm dev/build/package/test/doctor` |
| `create-kasumi` | 仅用于 `pnpm create kasumi`，不进入应用运行时 |

生成项目通过 `pnpm ksm build --windows`、`pnpm ksm build --android` 或
`pnpm ksm build --all` 构建。Windows 默认将 `frontend/dist/` 嵌入 EXE；Android
将 `frontend/dist/` 作为 `assets/` 打包。两者都不复制框架源码。CLI 通过根
`package.json` 的 `frontend:dev`、`frontend:build` 脚本调用框架；识别 Next.js 与
Nuxt 后将各自输出目录同步为统一的 `frontend/dist/`。

发布前先验证包内容：

```bash
pnpm test:packages
pnpm test:tarballs
pnpm build:bridge
pnpm build:plugins
pnpm --filter kasumi-cli pack
pnpm --filter kasumi-bridge pack
pnpm --filter create-kasumi pack
pnpm --filter kasumi-plugin-fs pack
pnpm --filter kasumi-plugin-dialog pack
pnpm --filter kasumi-plugin-clipboard pack
pnpm --filter kasumi-plugin-notification pack
pnpm --filter kasumi-plugin-window pack
pnpm --filter kasumi-plugin-http pack
pnpm --filter kasumi-plugin-shell pack
pnpm --filter kasumi-plugin-device pack
pnpm --filter kasumi-plugin-sql pack
pnpm --filter kasumi-plugin-path pack
pnpm --filter kasumi-plugin-store pack
pnpm --filter kasumi-plugin-os pack
pnpm --filter kasumi-plugin-log pack
```

用户项目执行 `pnpm ksm package --windows` 或 `pnpm ksm package --android` 时，产物写入
`.ksm/dist/packages/`。框架维护者执行 `pnpm release:prepare` 时，全部上传附件写入
`.ksm/release/kasumi/<version>/` 与 `.ksm/release/app/<version>/`。Android release 需要：

```text
KASUMI_ANDROID_KEYSTORE
KASUMI_ANDROID_KEYSTORE_PASSWORD
KASUMI_ANDROID_KEY_ALIAS
KASUMI_ANDROID_KEY_PASSWORD
```

标签发布工作流先生成并上传 GitHub Release 资产，再发布 `kasumi-bridge`、全部
`kasumi-plugin-*`、`kasumi-cli` 和 `create-kasumi`，并使用 `NPM_TOKEN` 与 npm
provenance。Release 资产包含
Windows SDK、Android Maven 仓库、AAR、示例 ZIP、APK 和 `kasumi-release.json`。
Android APK 签名始终要求四个 Android 凭据。Windows Authenticode 证书是可选项：
两个 Windows 证书 secret 同时存在时签名并校验示例 ZIP，都缺失时发布未签名产物，
只配置其中一个时 preflight 失败。缺少 `NPM_TOKEN` 时自动 npm 发布任务跳过，维护者
可在 GitHub Release 与 manifest 校验完成后手动发布同一提交的 npm 包。
Release 标签必须与 `version.json` 一致，即 `v<version>`。带预发布后缀的标签
会创建 GitHub prerelease，并将所有 npm 包发布到 `alpha` dist-tag，不覆盖 `latest`。

CI 使用 `pnpm install --frozen-lockfile`。根锁文件必须只包含根工程和 `packages/*`，
生成契约后执行 `git diff --exit-code`，避免临时项目依赖或未同步契约进入发布。

发布前必须先确认 GitHub Release 的 manifest 与 SDK 资产可访问，再按同一提交构建
npm tarball。正式发布使用 `NPM_TOKEN` 和 npm provenance；不要在 SDK Release 缺失时
提前发布 `create-kasumi`。

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
`notification`、`permission`、`event`、`window`、`http`、`shell`、`device`、`sql`、
`path`、`store`、`os`、`log` 和 `types`。`kasumi/plugins` 说明插件注册与扩展方式，
`kasumi/plugins/sql`、`path`、`store`、`os`、`log` 分别说明各插件的运行边界。
`kasumi/api` 只做索引，`kasumi/development` 记录当前实现进度。

线上页面以站点实际正文为准。`kasumi/development` 的修订需要审核和公网刷新后才能
视为生效；未在公网正文确认前只能视为草稿。`kasumi/configuration` 说明应用配置与
框架版本元数据的边界，API 页面按模块维护。

同步流程固定为：先用 `docs-rev.py pull` 拉取原文，再用本地编辑器修改，执行
`push --dry-run` 检查 diff，确认后推送修订，最后重新拉取或读取线上原文确认内容已生效。
未确认生效的页面不得写成已上线。
