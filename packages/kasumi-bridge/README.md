# kasumi-bridge

Kasumi 前端的 TypeScript 入口。它提供 `KSM` 根对象、模块 API、事件订阅、超时、
取消、请求大小限制和能力缓存。

```ts
import { KSM } from 'kasumi-bridge';

KSM.init();

const result = await KSM.ping();
const capabilities = await KSM.get();
```

`kasumi-bridge` 只通过宿主提供的 WebView 消息通道通信。页面来源校验、命令
allowlist、权限、路径守卫和平台限制仍由 Windows 或 Android 宿主执行。

文件系统、对话框和 SQL 另有独立插件包：

```ts
import { readText } from "kasumi-plugin-fs";
import { confirm } from "kasumi-plugin-dialog";
import { drivers } from "kasumi-plugin-sql";
```

`kasumi-bridge` 同时提供 `kasumi-bridge/fs`、`kasumi-bridge/dialog` 和
`kasumi-bridge/sql` 子路径，插件包通过它们复用同一套实现。
