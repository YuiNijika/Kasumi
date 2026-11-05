# kasumi-plugin-dialog

> 使用 Kasumi 原生消息框、确认框和文件选择器的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-dialog
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { confirm, openFile } from "kasumi-plugin-dialog";

KSM.init();

const confirmed = await confirm({
  title: "删除文件",
  message: "这个操作无法撤销。"
});

if (confirmed.confirmed) {
  const selected = await openFile({ filters: ["txt"] });
  console.log(selected.file);
}
```

文件选择器返回 `app://picked/<id>` 虚拟句柄，不暴露真实路径。`KSM.dialog`
继续作为兼容入口保留。

`openFolder` 返回包含 `id`、`path` 和 `files` 的目录 scope，`openFiles` 返回
包含 `id`、`path` 和多个文件句柄的结果；不暴露真实路径。
