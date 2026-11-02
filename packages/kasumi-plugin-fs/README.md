# kasumi-plugin-fs

> 使用 Kasumi 沙箱虚拟文件系统插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-fs
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { readText, writeText } from "kasumi-plugin-fs";

KSM.init();

await writeText({
  path: "app://data/example.txt",
  content: "hello"
});

const text = await readText({ path: "app://data/example.txt" });
```

这个包只提供前端调用入口。路径沙箱、权限、命令 allowlist 和平台检查仍由
Windows 或 Android 宿主执行。`KSM.fs` 继续作为兼容入口保留。
