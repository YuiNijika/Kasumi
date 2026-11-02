<table width="100%" align="center">
    <tr>
        <td>
            <img src="images/kasumi-logo-1.png" />
        </td>
    </tr>
</table>

<div align="center">
    <h2>KirakiraDokidoki</h2>
    一套 Web 前端构建更原生、轻量、快速的应用程序
</div>

Kasumi 提供统一的 TypeScript API、命令协议、权限校验、原生宿主和项目脚手架。前端可使用
Vite、React、Vue、Next.js 或 Nuxt，Windows 使用 Win32 + WebView2，Android 使用
WebView 与原生宿主服务。

## 快速开始

需要 Node.js 20 或更高版本，以及 pnpm 11。

### 创建应用

```bash
pnpm create kasumi
pnpm create kasumi my-app --all
pnpm create kasumi my-app --template vue
pnpm create kasumi my-app --template nextjs
```

交互式运行会先询问应用目录或名称，再显示 `basic`、`react`、`vue`、`nextjs`、
`nuxtjs` 模板菜单。传入 `--template <name>` 可跳过选择；`--yes` 使用默认应用名和
`basic` 模板。

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
pnpm ksm doctor
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
    "commands": "all",
    "decorations": true
  },
  "android": {
    "packageName": "dev.kasumi.my_app",
    "version": "0.1.0",
    "commands": "all"
  }
}
```

框架自身的发布版本只维护在 `version.json`。修改后运行 `pnpm version:sync` 同步 npm
包、根应用配置、vcpkg manifest 和 Python 包元数据；Windows CMake 与 Android Gradle
在构建时读取该文件并生成运行时版本常量。

`commands` 支持两种写法：

| 写法 | 行为 |
| --- | --- |
| `"all"` | 加载该平台协议矩阵中的全部命令，宿主仍会按平台过滤 |
| `["ping", "capabilities.query", "..."]` | 只启用显式列出的命令，必须包含两个核心命令 |

显式 allowlist 中出现重复命令、未知命令、跨平台命令或缺少 `ping`、
`capabilities.query` 时，配置校验会直接失败。`"all"` 会在配置加载阶段展开为当前平台的
完整命令列表，运行时行为与显式数组一致。

`windows.decorations` 控制 Windows 原生标题栏，默认值为 `true`。设置为 `false` 后，
系统标题栏会隐藏，但窗口边框、拖动边缘、最小尺寸和工作区约束仍然保留。Kasumi 不会
自动生成最小化、最大化和关闭按钮，页面需要自行提供这些控件并调用
`KSM.window.minimize()`、`KSM.window.maximize()` 和 `KSM.window.close()`。

自定义标题栏需要声明可拖拽区域，并把交互控件排除在拖拽区域之外：

```css
.titlebar {
  -webkit-app-region: drag;
  app-region: drag;
}

.titlebar button,
.titlebar input {
  -webkit-app-region: no-drag;
  app-region: no-drag;
}
```

Android 没有 `decorations` 字段，平台校验会拒绝该字段。

## 前端调用

```ts
import { KSM } from "kasumi-bridge";

KSM.init();

const [pong, capabilities, appInfo] = await Promise.all([
  KSM.ping(),
  KSM.get(),
  KSM.app.getInfo(),
]);

console.log(pong.pong, capabilities.commands, appInfo.version);
```

页面只通过宿主提供的消息通道通信。来源校验、命令 allowlist、权限、虚拟文件路径和平台限制
仍由 Windows 或 Android 宿主执行。生产模式只接受 `https://app.local`；开发 origin
仅在 Windows `dev` 或 Android debug APK 中启用。Android WebView 的非应用导航会被直接
拦截，外部链接必须通过 `KSM.shell.openExternal()` 打开，并由宿主配置精确 origin 白名单。

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `pnpm ksm dev` | 启动 Windows 开发宿主与前端开发服务器 |
| `pnpm ksm dev --android` | 通过 `adb reverse` 在 Android 上开发 |
| `pnpm ksm build` | 默认构建 Windows |
| `pnpm ksm build --windows` | 构建 Windows EXE |
| `pnpm ksm build --android` | 构建 Android APK |
| `pnpm ksm build --all` | 构建 Windows 与 Android |
| `pnpm ksm package --windows` | 生成 Windows 发布 ZIP |
| `pnpm ksm package --android` | 生成签名 Android release APK |
| `pnpm ksm package --all` | 同时生成两端发布包 |
| `pnpm release:prepare` | 构建并汇总当前版本的全部 Release 附件 |
| `pnpm ksm test` | 运行契约、配置和平台测试 |
| `pnpm ksm doctor` | 检查 SDK、工具链、设备与配置 |
| `pnpm version:show` | 输出 `version.json` 中的框架版本 |
| `pnpm version:sync` | 同步必须保留字面量的派生版本文件 |
| `pnpm version:check` | 检查版本元数据与派生文件是否一致 |
| `pnpm validate:protocol` | 校验协议事实源 |
| `pnpm codegen` | 生成三端契约 |
| `pnpm test:packages` | 测试三个发布包 |
| `pnpm test:tarballs` | 安装真实 npm tarball 并构建五套模板 |
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
.ksm/dist/windows/<windows.packageName>.exe
.ksm/dist/android/<android.packageName>-<android.version>.apk
.ksm/dist/packages/<windows.packageName>-<version>-windows.zip
.ksm/dist/packages/<android.packageName>-<version>-android.apk
.ksm/release/kasumi/<version>/kasumi-release.json
.ksm/release/kasumi/<version>/kasumi-windows-sdk-<version>.zip
.ksm/release/kasumi/<version>/kasumi-android-sdk-<version>.aar
.ksm/release/kasumi/<version>/kasumi-android-maven-<version>.zip
.ksm/release/app/<version>/kasumi_win32_example.exe
.ksm/release/app/<version>/kasumi-win32-example-<version>.zip
.ksm/release/app/<version>/kasumi-android-example-<version>.apk
```

Windows 默认将 `frontend/dist/` 嵌入 EXE。Android 将同一份前端产物打包进 APK 的
`assets/`。两种模式都不依赖运行时外部 Web 目录。

`.ksm/dist/` 用于用户项目自身的应用产物。框架维护者在 `.ksm/release/kasumi/<version>/`
汇总 SDK 和 manifest，在 `.ksm/release/app/<version>/` 汇总示例应用产物。执行
`pnpm release:prepare`
会在一次构建结束后统一生成两侧目录，避免手动从多个构建目录收集文件。
`pnpm ksm build --all --target example` 会把示例可执行文件和 APK 直接写入
`.ksm/release/app/<version>/`；`pnpm release:prepare` 再补充可分发的示例 ZIP。

## 发布包

| npm 包 | 作用 |
| --- | --- |
| `create-kasumi` | `pnpm create kasumi` 的项目初始化入口 |
| `kasumi-cli` | 用户项目的 `ksm dev/build/package/test/doctor` |
| `kasumi-bridge` | 前端 `KSM` API、事件总线与生成契约 |

Windows 发布 ZIP 默认包含 EXE、内嵌 Web 资源和运行说明；显式传入
`--external-web` 时附带 `web/`，传入 `--with-webview2-loader` 时附带
`WebView2Loader.dll`。

Android release 打包前必须提供以下环境变量：

```text
KASUMI_ANDROID_KEYSTORE
KASUMI_ANDROID_KEYSTORE_PASSWORD
KASUMI_ANDROID_KEY_ALIAS
KASUMI_ANDROID_KEY_PASSWORD
```

缺少任一项时 `ksm package --android` 会在构建前失败。APK 生成后会执行
`apksigner verify`，未通过校验的产物不会复制到 `.ksm/dist/packages/`。

GitHub Release 从 `.ksm/release/kasumi/<version>/` 上传框架附件，并从
`.ksm/release/app/<version>/` 上传示例附件。Windows SDK ZIP、Android Maven ZIP 和
`kasumi-release.json` 是初始化器的必需资产。
初始化器会校验 manifest 中的版本、文件大小和 SHA-256。
标签发布流程先完成 GitHub Release 资产和签名，再按 `kasumi-bridge`、`kasumi-cli`、
`create-kasumi` 的顺序发布 npm 包。普通 CI 不要求签名证书。

## 排查

1. 运行 `pnpm ksm doctor` 检查配置、SDK、CMake、Gradle、adb 和 WebView2。
2. Windows 缺少依赖时设置 `VCPKG_ROOT`，CLI 会使用项目内的 `windows/vcpkg.json`。
3. Android 无法解析 Kasumi SDK 时，确认 `vendor/android/repo/` 包含 `dev/kasumi/kasumi/`。
4. 页面空白时先确认 `frontend/dist/` 已生成，再检查宿主资源加载日志。
5. 命令返回 `NOT_SUPPORTED` 时，检查 `kasumi.config.json` 的 allowlist 和平台矩阵。
6. 初始化器无法下载 SDK 时，检查 GitHub Release 标签、资产文件名和 manifest。

## 许可证

Kasumi 使用 MIT License，版权署名为 `YuiNijika`。

## 文档与协议

- 开发文档：<https://blog.miomoe.cn/docs/kasumi/development>
- 协议说明：[`protocol/README.md`](protocol/README.md)
- 初始化器：[`packages/create-kasumi/README.md`](packages/create-kasumi/README.md)
- 构建 CLI：[`packages/kasumi-cli/README.md`](packages/kasumi-cli/README.md)
- 前端桥接：[`packages/kasumi-bridge/README.md`](packages/kasumi-bridge/README.md)
