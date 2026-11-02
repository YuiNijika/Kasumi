# kasumi-plugin-os

> 使用 Kasumi 非持久操作系统信息插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-os
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { getInfo } from "kasumi-plugin-os";

KSM.init();

const info = await getInfo();
console.log(info.platform, info.architecture, info.version);
```

返回平台、架构、系统版本、区域、时区和应用版本，不返回主机名、设备标识或用户信息。
Windows 的时区值是系统时区 ID，Android 的值是 IANA 时区 ID。
