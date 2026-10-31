<div align="center">
    <h2>KirakiraDokidoki</h2>
    一套 Web 前端构建更原生、轻量、快速的应用程序
</div>

<p align="center">
  <img src="images/kasumi-logo-1.png" alt="Kasumi" width="640" />
</p>

Kasumi 提供统一的 TypeScript API、命令协议、权限校验、原生宿主和项目脚手架。前端继续使用
Vite 与标准 Web 技术，Windows 使用 Win32 + WebView2，Android 使用 WebView 与原生宿主服务。

## 当前能力

| 能力 | 说明 |
| --- | --- |
| 命令协议 | 29 个命令、2 个事件，Windows 适用 28 个，Android 适用 25 个 |
| 前端 API | `KSM` 模块化 API、事件订阅、超时、取消和能力缓存 |
| Windows | Win32 + WebView2、内嵌 Web 资源、原生对话框、文件、窗口和下载 |
| Android | WebView、Activity 生命周期、文件选择、通知、设备信息和下载 |
| 项目入口 | `pnpm create kasumi` 初始化，`pnpm ksm` 开发、构建、打包和诊断 |
| 发布边界 | npm 包 + GitHub Release SDK，用户项目不复制框架源码 |

## 模块边界

| 路径 | 责任 |
| --- | --- |
| `frontend/` | `kasumi-bridge` 前端 API、生成契约和展示示例 |
| `windows/` | Windows SDK、原生路由、宿主服务和 WebView2 适配 |
| `android/` | Android AAR、原生路由、宿主服务和资源加载 |
| `protocol/` | 命令、事件、权限、错误码和配置 schema |
| `cli/` | 框架维护者使用的本地构建入口 |
| `packages/` | 发布用 `kasumi-bridge`、`kasumi-cli` 和 `create-kasumi` |
| `examples/` | Windows 与 Android 消费方示例 |

用户初始化后的项目只包含 `frontend/`、`windows/`、`android/`、`vendor/`、
`kasumi.config.json` 和一份根 `package.json`。框架源码通过 Release SDK 消费。

## 快速开始

需要 Node.js 20 或更高版本，以及 pnpm 11。

### 创建应用

```bash
pnpm create kasumi
pnpm create kasumi my-app --all
```

初始化器会生成前后端分离的项目、下载对应的 Windows SDK 与 Android Maven 仓库，并安装
`kasumi-bridge`、`kasumi-cli` 和前端依赖。

```bash
cd my-app
pnpm ksm doctor
pnpm ksm dev
pnpm ksm build
```

`ksm build` 默认构建 Windows。显式构建 Android 或双端：

```bash
pnpm ksm build --android
pnpm ksm build --all
```

### 从源码开发框架

```bash
pnpm install
pnpm typecheck
pnpm test:contract
pnpm ksm build --all
```

## 配置

根目录 `kasumi.config.json` 只保留应用开发需要的字段：

```json
{
  "$schema": "./protocol/kasumi.config.schema.json",
  "windows": {
    "packageName": "my-app",
    "version": "0.1.0",
    "commands": "all"
  },
  "android": {
    "packageName": "dev.kasumi.my_app",
    "version": "0.1.0",
    "commands": "all"
  }
}
```

`commands` 支持两种写法：

| 写法 | 行为 |
| --- | --- |
| `"all"` | 加载该平台协议矩阵中的全部命令，宿主仍会按平台过滤 |
| `["ping", "capabilities.query", "..."]` | 只启用显式列出的命令，必须包含两个核心命令 |

显式 allowlist 中出现重复命令、未知命令、跨平台命令或缺少 `ping`、
`capabilities.query` 时，配置校验会直接失败。`"all"` 会在配置加载阶段展开为当前平台的
完整命令列表，运行时行为与显式数组一致。

## 前端调用

```ts
import { KSM } from 'kasumi-bridge';

KSM.init();

const [pong, capabilities, appInfo] = await Promise.all([
  KSM.ping(),
  KSM.get(),
  KSM.app.getInfo(),
]);

console.log(pong.pong, capabilities.commands, appInfo.version);
```

页面只通过宿主提供的消息通道通信。来源校验、命令 allowlist、权限、虚拟文件路径和平台限制
仍由 Windows 或 Android 宿主执行。

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `pnpm ksm dev` | 启动 Windows 开发宿主与 Vite |
| `pnpm ksm dev --android` | 通过 `adb reverse` 在 Android 上开发 |
| `pnpm ksm build` | 默认构建 Windows |
| `pnpm ksm build --windows` | 构建 Windows EXE |
| `pnpm ksm build --android` | 构建 Android APK |
| `pnpm ksm build --all` | 构建 Windows 与 Android |
| `pnpm ksm package` | 生成平台发布包 |
| `pnpm ksm test` | 运行契约、配置和平台测试 |
| `pnpm ksm doctor` | 检查 SDK、工具链、设备与配置 |
| `pnpm validate:protocol` | 校验协议事实源 |
| `pnpm codegen` | 生成三端契约 |
| `pnpm test:packages` | 测试三个发布包 |
| `pnpm pack:packages` | 生成三个 npm tarball |

## 平台要求

| 平台 | 依赖 |
| --- | --- |
| Windows | Visual Studio C++ 工具链、CMake、vcpkg、Evergreen WebView2 Runtime |
| Android | JDK 21、Android SDK；项目自带 Gradle 8.14.3 Wrapper |

Windows 默认调用系统 Evergreen WebView2，不随应用生成 `WebView2Loader.dll` 或
`.WebView2` 数据目录。需要显式分发 Loader 时使用：

```bash
pnpm ksm build --windows --with-webview2-loader
```

## 构建产物

```text
dist/windows/<windows.packageName>.exe
dist/android/<android.packageName>-<android.version>.apk
```

Windows 默认将 `frontend/dist/` 嵌入 EXE。Android 将同一份前端产物打包进 APK 的
`assets/`。两种模式都不依赖运行时外部 Web 目录。

## 发布包

| npm 包 | 作用 |
| --- | --- |
| `create-kasumi` | `pnpm create kasumi` 的项目初始化入口 |
| `kasumi-cli` | 用户项目的 `ksm dev/build/package/test/doctor` |
| `kasumi-bridge` | 前端 `KSM` API、事件总线与生成契约 |

GitHub Release 需要上传 Windows SDK ZIP、Android Maven ZIP 和 `kasumi-release.json`。
初始化器会校验 manifest 中的版本、文件大小和 SHA-256。

## 排查

1. 运行 `pnpm ksm doctor` 检查配置、SDK、CMake、Gradle、adb 和 WebView2。
2. Windows 缺少依赖时设置 `VCPKG_ROOT`，CLI 会使用项目内的 `windows/vcpkg.json`。
3. Android 无法解析 Kasumi SDK 时，确认 `vendor/android/repo/` 包含 `dev/kasumi/kasumi/`。
4. 页面空白时先确认 `frontend/dist/` 已生成，再检查宿主资源加载日志。
5. 命令返回 `NOT_SUPPORTED` 时，检查 `kasumi.config.json` 的 allowlist 和平台矩阵。
6. 初始化器无法下载 SDK 时，检查 GitHub Release 标签、资产文件名和 manifest。

## 文档与协议

- 开发文档：<https://blog.miomoe.cn/docs/kasumi/development>
- 协议说明：[`protocol/README.md`](protocol/README.md)
- 初始化器：[`packages/create-kasumi/README.md`](packages/create-kasumi/README.md)
- 构建 CLI：[`packages/kasumi-cli/README.md`](packages/kasumi-cli/README.md)
- 前端桥接：[`packages/kasumi-bridge/README.md`](packages/kasumi-bridge/README.md)
