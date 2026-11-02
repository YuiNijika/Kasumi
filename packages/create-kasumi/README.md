# create-kasumi

创建并初始化一个 Kasumi 应用：

```bash
pnpm create kasumi
pnpm create kasumi@alpha
pnpm create kasumi my-app
pnpm create kasumi my-app --all
pnpm create kasumi my-app --template react
pnpm create kasumi my-app --template nextjs
```

交互式创建会先询问应用目录或名称，再显示模板菜单。输入序号或模板名均可，直接回车
使用 `basic`。传入 `--template <name>` 会跳过模板菜单。

初始化器会生成前后端分离的项目结构，安装 `kasumi-cli`、`kasumi-bridge` 和所选前端
框架依赖，并从对应 GitHub Release 拉取 Windows SDK 与 Android Maven 仓库。框架的
C++ 和 Kotlin 源码不会进入用户项目。

## 模板

| 模板 | 前端栈 | 构建输出 |
| --- | --- | --- |
| `basic` | TypeScript + Vite | `frontend/dist/` |
| `react` | React + TypeScript + Vite | `frontend/dist/` |
| `vue` | Vue + TypeScript + Vite | `frontend/dist/` |
| `nextjs` | Next.js App Router 静态导出 | `frontend/dist/` |
| `nuxtjs` | Nuxt 静态生成 | `frontend/dist/` |

`next` 与 `nuxt` 分别作为 `nextjs`、`nuxtjs` 的别名。React、Vue、Next.js 和 Nuxt
项目的根 `package.json` 由对应官方脚手架生成，再用包管理器补充 Kasumi 依赖和命令；
不会在初始化器里维护一份手写的框架依赖清单。

所有模板都提供两个内部脚本。`ksm` 通过它们启动开发服务器和构建前端，并在 Next.js、
Nuxt 输出目录不同的情况下统一生成 `frontend/dist/`：

```text
frontend:dev    启动固定 5173 端口的开发服务器
frontend:build  构建静态前端资源
```

模板内置三类构建所需的元数据：

- `pnpm-workspace.yaml` 允许 pnpm 执行 `esbuild` 安装脚本。
- `windows/vcpkg.json` 声明 `webview2` 与 `nlohmann-json`。
- `android/gradle/wrapper/` 与 `gradlew*` 固定 Gradle 8.14.3。

Nuxt 模板把 `glob` 与 `minimatch` override 写入 `pnpm-workspace.yaml`，兼容 pnpm 11
与后续版本把项目设置迁出 `package.json` 的配置变化。

## 命令选项

```text
--windows                 只启用 Windows
--android                 只启用 Android
--all                     同时启用 Windows 与 Android（默认）
--template <name>         basic、react、vue、nextjs、nuxtjs；省略时交互选择
--package-manager <name>  pnpm、npm 或 yarn
--no-install              不执行依赖安装
--no-sdk                  不下载平台 SDK
--force                   允许写入非空目录
--yes                     使用默认应用名和默认模板，不显示交互提示
--kasumi-version <ver>    Kasumi 版本，默认与 create-kasumi 一致
--release-base-url <url>  覆盖 Release 下载地址
```

生成后的项目保持一份根 `package.json`：

```text
my-app/
├─ package.json
├─ pnpm-workspace.yaml
├─ kasumi.config.json
├─ frontend/
├─ windows/
│  └─ vcpkg.json
├─ android/
│  ├─ gradlew
│  ├─ gradlew.bat
│  └─ gradle/wrapper/
└─ vendor/
```

`kasumi.config.json` 的平台 `commands` 默认使用 `"all"`，加载时按协议矩阵展开。需要收窄
能力时改为显式数组，并保留 `ping` 与 `capabilities.query`：

```json
{
  "windows": {
    "commands": ["ping", "capabilities.query", "app.getInfo"]
  },
  "android": {
    "commands": ["ping", "capabilities.query", "device.getInfo"]
  }
}
```

`"all"` 同时启用 `sql.drivers`、`sql.open`、`sql.query`、`sql.execute` 和
`sql.close`。生成项目的 Windows 模板包含 `sqlite3` vcpkg 依赖，SQLite 可直接使用。
Windows 会另外发现系统已安装的 MySQL / MariaDB 和 PostgreSQL ODBC 驱动；没有厂商
驱动时这两个标识不会出现在 `sql.drivers`。MongoDB 和 Android 远程数据库需要通过
`KSMSqlDriver` SPI 注册宿主驱动。

Windows 还可以设置 `"decorations": false` 隐藏系统标题栏。此时应用需要自行提供最小化、
最大化和关闭按钮，并在页面中声明可拖拽的标题栏区域：

```css
.titlebar {
  -webkit-app-region: drag;
  app-region: drag;
}

.titlebar button {
  -webkit-app-region: no-drag;
  app-region: no-drag;
}
```

Android 没有该字段，配置中写入 `decorations` 会被拒绝。

## SDK 下载

默认 Release 地址为：

```text
https://github.com/YuiNijika/Kasumi/releases/download/v<version>
```

初始化器只允许 HTTPS，测试模式额外允许回环地址 HTTP。manifest 最大 1 MiB，单个资产
最大 512 MiB，总超时 10 分钟，最多跟随 5 次重定向。下载会同步计算 SHA-256，并在
项目内 `.tmp/` 暂存目录中校验 ZIP 布局、绝对路径、父目录穿越和符号链接。Windows 与
Android SDK 全部校验通过后才统一提交；任一目标提交失败会回滚本次安装并保留旧 SDK。

初始化中途失败时，本次新建的项目目录会被删除。使用 `--no-sdk` 可跳过平台 SDK 下载，
但后续原生构建需要手动补齐 `vendor/windows/kasumi/` 与 `vendor/android/repo/`。

## 发布

标签 `v<version>` 会触发 npm 包发布和 GitHub Release 资产生成。发布前先在本地
检查 tarball：

```bash
pnpm test:packages
pnpm test:tarballs
pnpm build:bridge
pnpm pack:packages
```

npm 发布需要仓库配置 `NPM_TOKEN`。GitHub Release 需要包含
`kasumi-release.json`、Windows SDK ZIP 和 Android Maven ZIP，初始化器会校验
manifest 中的 SHA-256。正式标签还要求 Windows Authenticode 证书和 Android keystore；
发布工作流只有在 GitHub Release 资产上传成功后才发布 npm 包。
