# kasumi-plugin-log

> 使用 Kasumi 平台日志插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-log
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { write } from "kasumi-plugin-log";

KSM.init();

await write({
  level: "info",
  target: "application",
  message: "应用已启动"
});
```

日志消息最多 64 KiB，目标最多 128 字节。Windows 写入调试输出，Android 写入
`android.util.Log`；日志不会自动上传或持久化。
