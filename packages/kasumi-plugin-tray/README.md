# kasumi-plugin-tray

> 控制系统托盘图标。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-tray
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { show, hide } from "kasumi-plugin-tray";

KSM.init();

await show();
await hide();
```

当前仅在 Windows 宿主提供支持。
