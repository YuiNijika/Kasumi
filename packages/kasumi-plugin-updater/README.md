# kasumi-plugin-updater

> 检查并下载 Kasumi 应用更新包。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-updater
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { check, download } from "kasumi-plugin-updater";

KSM.init();

const update = await check();
if (update.available) {
  await download({ version: update.version });
}
```
