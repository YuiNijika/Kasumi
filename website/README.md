# Kasumi 官网

> 使用 Vue 3、Vite 和 Composition API 构建的 Kasumi 单页官网。

## 模块边界

| 目录 | 职责 |
| --- | --- |
| `src/App.vue` | 页面区块组合和进入视口动画 |
| `src/data/site.ts` | 导航、Hero、平台、技术栈、开发流程、优势和对比数据 |
| `src/components/*.vue` | 单文件页面区块和复制、代码展示组件 |
| `src/useTheme.ts` | 日间与夜间模式切换、主题持久化和浏览器主题色同步 |
| `src/styles/tokens.css` | 日夜主题、橙色品牌色和排版变量 |
| `src/styles/base.css` | 全局排版、焦点、动画和响应式基础 |

组件只负责渲染 `site.ts` 提供的内容。新增平台、框架或对比项时先修改数据文件，不要在组件
中复制文案。

视觉方向采用橙色品牌色、清晰的工程边界、克制排版和低装饰密度。页面导航通过
`data-section` 定位并调用 `scrollIntoView`，不会把 `#section` 写入地址栏。组件保留为
`src/components/ComponentName.vue`，不再为每个组件建立同名目录。

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
