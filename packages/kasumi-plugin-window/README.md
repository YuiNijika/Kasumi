# kasumi-plugin-window

> 使用 Kasumi 窗口控制插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-window
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { close, setSize, setTitle } from "kasumi-plugin-window";

KSM.init();

await setTitle("Kasumi");
await setSize({ width: 960, height: 720 });
await close();
```

Windows 支持标题、最小化、最大化、尺寸和关闭。Android 只提供 `close`，其余命令返回
`NOT_SUPPORTED`。`KSM.window` 继续作为兼容入口。
