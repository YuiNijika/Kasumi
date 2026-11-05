# kasumi-plugin-media-session

> 使用 Kasumi 媒体会话元数据与传输控制事件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-media-session
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { clear, onCommand, update } from "kasumi-plugin-media-session";

KSM.init();

const unsubscribe = onCommand(({ command }) => console.info(command));
await update({ title: "Track", state: "playing" });
await clear();
unsubscribe();
```

媒体通知、锁屏按钮和前台服务归宿主实现；插件包只更新元数据并订阅
`mediaSession.command`。`KSM.mediaSession` 继续作为兼容入口。
