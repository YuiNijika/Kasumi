# kasumi-plugin-global-shortcut

> 注册和注销系统级快捷键。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-global-shortcut
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { register, unregister } from "kasumi-plugin-global-shortcut";

KSM.init();

await register({ shortcut: "Ctrl+Shift+K" });
await unregister({ shortcut: "Ctrl+Shift+K" });
```

当前仅在 Windows 宿主提供支持。

`onTriggered` 订阅 `globalShortcut.triggered`。注册失败必须返回
`{ registered: false }`，不能把占位行为伪装成成功。
