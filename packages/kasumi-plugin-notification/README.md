# kasumi-plugin-notification

> 使用 Kasumi 系统通知插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-notification
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { show } from "kasumi-plugin-notification";

KSM.init();

await show({
  title: "构建完成",
  body: "Windows 和 Android 产物已写入 .ksm"
});
```

`KSM.notification` 继续作为兼容入口。Android 运行时通知权限仍由宿主检查。
