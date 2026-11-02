# kasumi-plugin-shell

> 使用 Kasumi 白名单外部链接插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-shell
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { openExternal } from "kasumi-plugin-shell";

KSM.init();

await openExternal({ url: "https://example.com/docs" });
```

宿主默认拒绝所有 origin，应用必须在初始化时配置精确白名单。WebView 内部导航不能绕过
该入口。`KSM.shell` 继续作为兼容入口。
