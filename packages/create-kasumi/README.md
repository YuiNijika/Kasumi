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
--kasumi-version <ver>    Kasumi 版本，默认 0.1.0
--release-base-url <url>  覆盖 Release 下载地址
```

生成后的项目保持一份根 `package.json`：

```text
my-app/
├─ package.json
├─ kasumi.config.json
├─ frontend/
├─ windows/
├─ android/
└─ vendor/
```

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
