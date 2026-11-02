# kasumi-plugin-path

> 使用 Kasumi 虚拟路径插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-path
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { dirname, dirs, join } from "kasumi-plugin-path";

KSM.init();

const roots = await dirs();
const file = await join({
  base: roots.data,
  segments: ["notes", "entry.txt"]
});
const parent = await dirname({ path: file.path });
```

该插件只处理 `app://` 虚拟路径，不接受真实文件系统路径。它不会读取或写入文件，
也不会绕过 `fs` 插件的权限和沙箱检查。
