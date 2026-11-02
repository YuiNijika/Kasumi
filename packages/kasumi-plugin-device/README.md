# kasumi-plugin-device

> 使用 Kasumi 非持久设备信息插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-device
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { getInfo } from "kasumi-plugin-device";

KSM.init();

const info = await getInfo();
console.log(info.model, info.osVersion);
```

`device.getInfo` 仅 Android 可用，不返回 Android ID、序列号、IMEI、广告 ID 或其他持久
设备标识。`KSM.device` 继续作为兼容入口。
