# kasumi-plugin-store

> 使用 Kasumi 持久化 JSON 存储插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-store
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { get, set } from "kasumi-plugin-store";

KSM.init();

await set({
  key: "profile",
  value: {
    name: "Kasumi",
    compact: true
  }
});

const profile = await get({ key: "profile" });
if (profile.found) {
  console.log(profile.value);
}
```

数据写入 `app://data/.kasumi/store.json`。根值必须是 JSON 对象，单个值最多 1 MiB，
键最多 256 字节。
