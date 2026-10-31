# create-kasumi

创建并初始化一个 Kasumi 应用：

```bash
pnpm create kasumi
pnpm create kasumi my-app
pnpm create kasumi my-app --all
```

初始化器会生成前后端分离的项目结构，安装 `kasumi-cli`、`kasumi-bridge` 和 Vite，
并从对应 GitHub Release 拉取 Windows SDK 与 Android Maven 仓库。框架的 C++ 和
Kotlin 源码不会进入用户项目。

模板内置三类构建所需的元数据：

- `pnpm-workspace.yaml` 允许 pnpm 执行 `esbuild` 安装脚本。
- `windows/vcpkg.json` 声明 `webview2` 与 `nlohmann-json`。
- `android/gradle/wrapper/` 与 `gradlew*` 固定 Gradle 8.14.3。

## 命令选项

```text
--windows                 只启用 Windows
--android                 只启用 Android
--all                     同时启用 Windows 与 Android（默认）
--template basic          使用基础模板
--package-manager <name>  pnpm、npm 或 yarn
--no-install              不执行依赖安装
--no-sdk                  不下载平台 SDK
--force                   允许写入非空目录
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

## 发布

标签 `v<version>` 会触发 npm 包发布和 GitHub Release 资产生成。发布前先在本地
检查 tarball：

```bash
pnpm test:packages
pnpm build:bridge
pnpm pack:packages
```

npm 发布需要仓库配置 `NPM_TOKEN`。GitHub Release 需要包含
`kasumi-release.json`、Windows SDK ZIP 和 Android Maven ZIP，初始化器会校验
manifest 中的 SHA-256。
