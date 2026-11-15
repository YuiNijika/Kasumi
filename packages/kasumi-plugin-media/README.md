# kasumi-plugin-media

> 使用 Kasumi 媒体探测与外部工具发现能力的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-media
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { detectFfmpeg, probe } from "kasumi-plugin-media";

KSM.init();

const metadata = await probe({ path: "app://data/track.flac" });
const ffmpeg = await detectFfmpeg();
console.info(metadata.title, ffmpeg.available);
```

媒体探测只接受虚拟路径或应用配置允许的 HTTPS 地址。FFmpeg 路径由宿主校验，
`KSM.media` 继续作为兼容入口。
