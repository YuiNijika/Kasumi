# kasumi-plugin-audio

> 使用 Kasumi 通用音频播放与设备控制插件的独立 TypeScript 入口。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-audio
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { load, play, onTick, setSpeed } from "kasumi-plugin-audio";

KSM.init();

const unsubscribe = onTick(({ positionMs }) => console.info(positionMs));
await load({ source: "app://data/track.bin" });
await setSpeed({ speed: 1.25 });
await play();
unsubscribe();
```

音源只能是虚拟路径或受配置约束的 HTTPS URL。设备枚举、EQ、倍速、seek 和
播放状态通过同一宿主服务执行。`KSM.audio` 继续作为兼容入口。
