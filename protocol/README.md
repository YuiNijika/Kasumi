# Kasumi protocol

> 说明协议契约的事实源、参数校验和跨端生成入口。

## 模块边界

`protocol/` 只负责定义跨端契约，不负责实现命令、执行权限校验或发送 WebView 消息。

| 文件 | 责任 |
| --- | --- |
| `commands.json` | 当前命令、事件、参数、返回结构和平台支持 |
| `commands.schema.json` | 命令、事件和参数结构的 JSON Schema 校验规则 |
| `errors.json` | 双端共用的错误码全集 |
| `permissions.schema.json` | 权限声明 |

Windows、Android 和前端实现都必须以这些文件为准。

## 参数与生成

`commands.json` 中的 `args` 和 `result` 使用 JSON Schema 子集描述。当前生成器支持：

- `type`、`properties`、`required`、`additionalProperties`
- `items`、`enum`、`const`
- `minLength`、`maxBytes`

运行 `python tools/codegen.py` 后，三端会得到同一份命令、事件、权限和参数校验器。不要在宿主或前端手写平行的命令清单。

## 核心命令

`commands.json` 当前定义 25 个内置命令和 2 个事件。`ping` 与
`capabilities.query` 是最小通路，其余命令覆盖文件、对话框、剪贴板、通知、权限、
事件和窗口控制。

当前 Windows 与 Android 宿主已实现 `commands.json` 中的全部命令，并按平台过滤
`window.setTitle`、`window.minimize`、`window.maximize`。`capabilities.query`
返回宿主实际实现的命令列表和当前可用权限。

`fs.readText` 的结果是 `string` 或 `{ "file": "app://tmp/read-<id>.txt" }`。
超过 64KiB 的结果不会塞进单条消息，而是写入临时文件；再次读取该临时句柄时返回同一句柄。
`app://data`、`app://cache`、`app://tmp` 可读写，`app://res` 与 `app://assets`
只读。

文件系统还提供目录与元数据方法：

- `fs.readDir` 返回目录项数组，目录项按名称排序；可读根包括 `data`、`cache`、`tmp`、
  `res` 和 `assets`，不接受 `app://picked`。
- `fs.mkdir` 只允许在 `data`、`cache`、`tmp` 下创建目录，`recursive` 缺省为 `false`。
- `fs.stat` 可查询可写沙箱、只读资源和有效 `app://picked/<id>`，返回
  `kind`、`size` 和 `modifiedAt`。

目录项和元数据中的 `modifiedAt` 是 Unix 毫秒或 `null`，目录 `size` 固定为 `0`。
宿主不跟随符号链接，符号链接项不会出现在 `readDir` 结果中，`stat` 视为不可用。

`webview.reloaded` 是系统事件，页面重载后由宿主主动推送，不需要订阅；
`window.resized` 仍通过 `event.subscribe` 订阅。

Protocol names do not use the `KSM` prefix. Host-side types and entry points use the `KSM` prefix.
Windows 与 Android 宿主侧通过 `KSM` 根入口执行生命周期、统一调用和事件监听，命令仍使用
`commands.json` 中的协议字符串。

宿主根入口按领域挂载子模块，并与协议命令保持同名映射：

| 协议命令 | Windows | Android |
| --- | --- | --- |
| `app.getInfo` / `app.exit` | `KSM::App::GetInfo` / `Exit` | `KSM.app.getInfo` / `exit` |
| `fs.*` | `KSM::Fs` | `KSM.fs` |
| `clipboard.*` | `KSM::Clipboard` | `KSM.clipboard` |
| `notification.show` | `KSM::Notification::Show` | `KSM.notification.show` |
| `permission.request` | `KSM::Permission::Request` | `KSM.permission.request` |
| `window.*` | `KSM::Window` | `KSM.window.close` |

Android 的平台命令清单不包含 `window.setTitle`、`window.minimize` 和 `window.maximize`，
因此 Android 根入口不提供这三个方法。`app.getInfo` 的版本和语言由各端宿主服务提供。

Each request receives exactly one response. Errors use the codes in `errors.json`.

`dialog.openFile` 和 `dialog.saveFile` 返回 `app://picked/<id>`。后续 `fs.readText`、
`fs.writeText`、`fs.exists`、`fs.remove` 可以直接使用该句柄，宿主不得把真实文件路径或
`content://` URI 返回给页面。

宿主根入口的直接文件子模块也遵循同一虚拟路径边界。Windows `KSM::Fs` 和 Android
`KSMFs` 都支持可写沙箱、只读资源与 `app://picked`，不提供绕过协议的第三个文件 API。

## 调用示例

前端通过 `frontend/src/bridge/ksm.ts` 暴露的 `KSM` 根入口调用：

```ts
KSM.initialize();
const result = await KSM.ping();
const capabilities = await KSM.get();
```

读取大文本时按返回类型处理：

```ts
const result = await KSM.fs.readText({ path: 'app://data/large.txt' });
if (typeof result === 'string') {
  console.log(result);
} else {
  console.log(result.file);
}

const { entries } = await KSM.fs.readDir({ path: 'app://data' });
const metadata = await KSM.fs.stat({ path: entries[0]?.path ?? 'app://data' });
```

宿主实现必须把请求转换为标准响应：

```json
{
  "type": "response",
  "id": "req-1",
  "ok": true,
  "data": {
    "pong": true
  }
}
```

## 排查路径

1. 确认命令已经写入 `commands.json`，并运行 `python tools/validate_protocol.py`。
2. 确认命令名没有添加 `KSM` 前缀。
3. 确认宿主返回的 `id` 与请求一致。
4. 确认错误码属于 `errors.json`。
5. 确认前端已先调用 `KSM.initialize`，并用 `KSM.get()` 读取缓存能力。
6. 确认超过 64KiB 的文本返回 `{ file }`，且临时句柄仍指向 `app://tmp/`。
7. 确认 `webview.reloaded` 由宿主系统事件触发，而不是页面自行订阅。
8. 确认 Android 发布构建的四个签名环境变量都已设置，并通过 `apksigner verify`。
9. 确认 `fs.readDir`、`fs.mkdir` 和 `fs.stat` 的根路径与子路径经过同一套路径守卫。
