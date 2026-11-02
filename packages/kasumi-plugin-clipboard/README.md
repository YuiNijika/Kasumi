# kasumi-plugin-clipboard

> 使用 Kasumi 系统剪贴板插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-clipboard
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { readText, writeText } from "kasumi-plugin-clipboard";

KSM.init();

await writeText("hello");
const { text } = await readText();
```

`KSM.clipboard` 继续作为兼容入口。剪贴板读取和写入权限仍由宿主检查。
