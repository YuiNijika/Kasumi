# Kasumi 项目规则

本项目包含两个概念，先区分场景再动手：

## 项目定位

- **kasumi-framework**：指本仓库 `D:\Codes\Kasumi` 本身，负责协议、bridge、插件、CLI、Windows/Android 宿主、文档、测试和发布。只有修改框架内部源码、协议契约、生成器、发布链路时才属于框架开发。
- **kasumi-app**：指使用 Kasumi SDK 构建的独立应用工程，例如 `pnpm create kasumi` 生成的工程，或任何消费 `kasumi-bridge` / `kasumi-cli` 的业务项目。

两个技能统一只从线上读取，不再使用本地镜像。线上地址与读取方式见下文。

# 开发场景 Skill 读取

任务开始时，先只读浏览线上 Skill 首页：

- 框架内部任务：浏览 `https://blog.miomoe.cn/docs/skill/kasumi/framework.mdx`
- 应用开发任务：浏览 `https://blog.miomoe.cn/docs/skill/kasumi/app.mdx`
- 在 Kasumi 仓库写代码或写技术文档：必须先浏览 `https://blog.miomoe.cn/docs/skill/kasumi/yuinijika.mdx`

线上 URL 后缀 `.mdx` 会直接返回 Markdown，不要求先 pull 到本地，也不要再尝试读取仓库内的 `docs/skill/kasumi/*.mdx` 镜像。

## 两个 Skill 的触发条件

### kasumi-app

触发条件：
- 用户想初始化、开发、调试、打包一个 Kasumi 应用
- 使用 `ksm dev`、`ksm build`、`ksm package`、`pnpm create kasumi`
- 读取 `kasumi.config.json`、配置 permissions / commands / origins
- 修改前端 `KSM.*` 调用，或修改生成工程中的 `windows/`、`android/` 业务代码

操作步骤：
1. 浏览 `https://blog.miomoe.cn/docs/skill/kasumi/app.mdx`
2. 按应用技能和线上文档执行。线上技能是当前唯一事实源
3. 实现后运行 `pnpm typecheck` 或 `pnpm ksm test` 验证，再按项目 `package.json` 脚本停止

### kasumi-framework

触发条件：
- 修改 `protocol/`、`tools/codegen.py`、`tools/validate_protocol.py`
- 修改 `frontend/`、`windows/`、`android/`、`packages/`、`cli/` 中属于框架实现的代码
- 修改协议契约、生成物、CMake、Gradle、packages 测试
- 发布框架 npm 包或 GitHub Release

操作步骤：
1. 浏览 `https://blog.miomoe.cn/docs/skill/kasumi/framework.mdx`
2. 如需 YuiNijika 编码与文档风格，继续浏览 `https://blog.miomoe.cn/docs/skill/kasumi/yuinijika.mdx`
3. 涉及前端 UI/交互时，按下方的前端 skill 规范执行
4. 实现后运行 `pnpm typecheck`、`pnpm validate:protocol`、`pnpm test:contract`、`pnpm test:packages`，修改 Windows/Android 原生时补跑平台测试

# 通用硬性约束

- 临时文件只放 `.tmp/`，禁止使用或创建 `.workbuddy/`
- 不用全仓格式化器批量重排无关文件；缩进跟随所在目录既有风格
- 协议 / 命令 / 权限 / 插件只能通过 `protocol/*` + `tools/codegen.py` 修改，不直接手改三端生成契约
- 在线文档变更只能使用 `pnpm ksm docs`（唯一实现 `tools/update_docs.py`），拉取和提交文件只允许位于 `.tmp/kasumi-docs/`
- 不在本地保存线上 skill 镜像，Skill 内容变化只通过线上 MDX 维护

# 前端 UI 规范

改动前端组件 / 页面 / 交互前必须读取：
- `web-design-guidelines`：先按 vercel-labs/web-interface-guidelines 最新规范设计，实现后逐条自检并输出 findings
- `web-artifacts-builder`：避免模板化 AI 风格，保持克制排版，移动端遵循触控与安全区规范
- 手势驱动 UI、弹簧动画、拖拽 / 滑动 / sheet 交互、半透明材质：先读取 `apple-design`，按 Apple 动效与材质规范实现

# 常见任务流程

## 新增命令 / 权限 / 插件

1. 先浏览 `https://blog.miomoe.cn/docs/skill/kasumi/framework.mdx`
2. 修改 `protocol/commands.json`、`protocol/permissions.schema.json`、`protocol/plugins.json`
3. 运行 `pnpm validate:protocol`
4. 运行 `pnpm codegen` 并将生成契约纳入改动
5. 实现双端路由/宿主/插件，补前端 API 与 plugin 包
6. 跑 `pnpm typecheck`、`pnpm test:contract`、平台测试

## 修改应用配置

1. 以 `protocol/kasumi.config.schema.json` 为字段事实源
2. 允许的字段同时更新 Node CLI（`packages/kasumi-cli/src/config.mjs`）和 Python CLI（`cli/kasumi_config.py`）
3. 模板默认 `kasumi.config.json` 同步加入可选项示例
4. `pnpm doctor` 对缺 permissions 等配置输出可操作提示，不自动改动用户文件

## 构建与分发

- SDK / 框架产物：`pnpm ksm package --target sdk`
- 应用产物：`pnpm ksm build` / `pnpm ksm package`
- Windows 安装包：`pnpm ksm package --installer nsis|msi`
- Android release：必须配齐 `KASUMI_ANDROID_KEYSTORE` 等签名变量
- 发布前必须确认版本与 `version.json` 一致

## 文档更新

1. `pnpm ksm docs pull` 将线上 MDX 拉到 `.tmp/kasumi-docs`
2. 本地编辑后运行 `--dry-run`
3. 正式推送必须带 `--note`
4. 推送后再次 `pnpm ksm docs pull` 验证线上生效

# GitHub Release 风格

发布 GitHub Release 时严格按以下结构，不写版本号 / 日期 / 作者：

1. 总述段落（2-4 个主题）
2. `## 新增功能`
3. `## 优化与调整`
4. `## 修复问题`

格式要求：列表项 `- `，小节标题 `## `，列表项之间不空行；“修复问题”每项以“修复”开头并写清现象 + 根因或方案；内容结合 commit 历史与项目记忆，只写真实改动。
