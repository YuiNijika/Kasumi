# kasumi-bridge

Kasumi 前端的 TypeScript 入口。它提供 `KSM` 根对象、模块 API、事件订阅、超时、
取消、请求大小限制和能力缓存。

```ts
import { KSM } from 'kasumi-bridge';

KSM.init();

const result = await KSM.ping();
const capabilities = await KSM.get();
```

扩展清单通过 `KSM.configureExtensions(contract)` 安装运行时命令与事件
allowlist。`KSM.extensions.invoke()`、`KSM.extensions.on()` 和
`KSM.emit()` 共享同一套校验与 `KSM.invoke` 请求链路。

`kasumi-bridge` 只通过宿主提供的 WebView 消息通道通信。页面来源校验、命令
allowlist、权限、路径守卫和平台限制仍由 Windows 或 Android 宿主执行。

内置模块均可通过独立插件包导入：

```ts
import { readText } from "kasumi-plugin-fs";
import { confirm } from "kasumi-plugin-dialog";
import { readText as readClipboard } from "kasumi-plugin-clipboard";
import { show } from "kasumi-plugin-notification";
import { setSize } from "kasumi-plugin-window";
import { download } from "kasumi-plugin-http";
import { openExternal } from "kasumi-plugin-shell";
import { getInfo } from "kasumi-plugin-device";
import { drivers } from "kasumi-plugin-sql";
import { join } from "kasumi-plugin-path";
import { get as getStored } from "kasumi-plugin-store";
import { getInfo as getOperatingSystem } from "kasumi-plugin-os";
import { write as writeLog } from "kasumi-plugin-log";
```

`kasumi-bridge` 同时提供 `fs`、`dialog`、`clipboard`、`notification`、`window`、
`http`、`shell`、`device`、`sql`、`path`、`store`、`os` 和 `log` 子路径，插件包通过
它们复用同一套实现。`KSM.fs`、`KSM.dialog`、`KSM.path`、`KSM.store`、`KSM.os` 和
`KSM.log` 等根对象继续作为兼容入口。
