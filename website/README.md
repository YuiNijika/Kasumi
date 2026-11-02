# Kasumi 官网

> 使用 Vue 3、Vite、Tailwind CSS 和 Composition API 构建的 Kasumi 单页官网。

## 模块边界

| 目录 | 职责 |
| --- | --- |
| `src/App.vue` | 页面区块组合和进入视口动画 |
| `src/data/site.ts` | 导航、Hero、平台、技术栈、开发流程、优势和对比数据 |
| `src/components/*.vue` | 单文件页面区块和复制、代码展示组件 |
| `src/useTheme.ts` | 日间与夜间模式切换、主题持久化和浏览器主题色同步 |
| `src/styles/tokens.css` | Tailwind 入口、日夜主题、橙色品牌色、排版与断点映射 |
| `src/styles/base.css` | 全局排版、焦点、动画和无障碍媒体查询 |

组件只负责渲染 `site.ts` 提供的内容。新增平台、框架或对比项时先修改数据文件，不要在组件
中复制文案。

视觉方向采用橙色品牌色、清晰的工程边界、克制排版和低装饰密度。页面导航通过
`data-section` 定位并调用 `scrollIntoView`，不会把 `#section` 写入地址栏。组件保留为
`src/components/ComponentName.vue`，不再为每个组件建立同名目录。

页面布局、颜色、间距和响应式行为使用 Tailwind CSS 4 工具类。`tokens.css` 通过
`@theme inline` 暴露品牌色、字体、圆角、阴影和断点；组件只在无法用工具类表达的
浏览器伪元素或第三方生成内容上保留必要样式。

## 开发

```sh
pnpm install
pnpm dev
```

## 验证与构建

```sh
pnpm type-check
pnpm build
pnpm preview
```

生产文件输出到 `website/dist`。

## 自动发布

`.github/workflows/website.yml` 在 `main` 分支的 `website/**` 变更后执行类型检查和生产
构建，然后把 `website/dist` 发布到 `website` 分支。需要 `workflow_dispatch` 手动重跑时，
在 Actions 中选择 `Kasumi website`。

仓库的 GitHub Pages Source 需要选择 `Deploy from a branch`，分支选择 `website`，目录选择
`/ (root)`。Vite 使用相对资源路径，因此站点可以部署在仓库子路径或自定义域名下。

| 项目 | 值 |
| --- | --- |
| 源分支 | `main` |
| 发布分支 | `website` |
| Node.js | 24 |
| pnpm | 11.25.0 |
| 构建目录 | `website/dist` |
