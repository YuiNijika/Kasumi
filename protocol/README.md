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

Phase 1 定义两个平台无关的核心命令：

| Command | Arguments | Result |
| --- | --- | --- |
| `ping` | `{}` | `{ "pong": true }` |
| `capabilities.query` | `{}` | Platform, supported commands, and permissions |

当前 Windows 与 Android 宿主已实现 `commands.json` 中的全部命令，并按平台过滤
`window.setTitle`、`window.minimize`、`window.maximize`。`capabilities.query`
返回宿主实际实现的命令列表和当前可用权限。

Protocol names do not use the `KSM` prefix. Host-side types and entry points use the `KSM` prefix.

Each request receives exactly one response. Errors use the codes in `errors.json`.

`dialog.openFile` 和 `dialog.saveFile` 返回 `app://picked/<id>`。后续 `fs.readText`、
`fs.writeText`、`fs.exists`、`fs.remove` 可以直接使用该句柄，宿主不得把真实文件路径或
`content://` URI 返回给页面。

## 调用示例

前端通过 `frontend/src/bridge/ksm.ts` 暴露的 `KSM` 根入口调用：

```ts
KSM.initialize();
const result = await KSM.ping();
const capabilities = await KSM.get();
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
