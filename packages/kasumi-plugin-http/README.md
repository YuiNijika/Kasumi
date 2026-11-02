# kasumi-plugin-http

> 使用 Kasumi HTTPS 下载插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-http
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { download } from "kasumi-plugin-http";

KSM.init();

const result = await download({
  url: "https://example.com/archive.zip",
  path: "app://data/archive.zip"
});

console.log(result.bytes);
```

下载只接受 HTTPS 和可写虚拟路径。`overwrite` 默认关闭，取消和超时继续复用请求链路。
`KSM.http` 继续作为兼容入口。
