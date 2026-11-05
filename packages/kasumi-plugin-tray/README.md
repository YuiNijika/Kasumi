# kasumi-plugin-tray

> 控制系统托盘图标。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-tray
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { show, hide } from "kasumi-plugin-tray";

KSM.init();

await show();
await hide();
```

当前仅在 Windows 宿主提供支持。

`configure` 设置图标、提示和菜单项；`onAction` 与 `onClick` 订阅托盘交互。
宿主必须返回真实状态，不能用占位成功代替未实现行为。
