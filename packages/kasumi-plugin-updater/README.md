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

`install` 触发宿主签名验证和原子安装；`onProgress` 与 `onStatus` 订阅
安装进度和状态。未配置更新源时不能返回伪造成功。
