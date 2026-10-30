# Kasumi protocol

> 说明协议契约的事实源、平台矩阵、参数校验、权限和跨端生成入口。

## 模块边界

`protocol/` 只定义跨端契约，不实现命令、执行权限或发送 WebView 消息。

| 文件 | 责任 |
| --- | --- |
| `commands.json` | 命令、事件、参数、返回结构、权限和平台支持 |
| `commands.schema.json` | 命令、事件和参数结构的 JSON Schema 校验规则 |
| `errors.json` | 双端共用的错误码全集 |
| `permissions.schema.json` | 权限声明 |

Windows、Android 和前端实现都必须以这些文件为准。协议字符串不加 `KSM` 前缀。

## 当前规模

`commands.json` 当前定义 **29 个命令和 2 个事件**。产品版本保持 `0.1.0`，新增命令使用
`since: "1.1.0"`。

| 平台 | 适用命令 | 说明 |
| --- | ---: | --- |
| Windows | 28 | 不包含仅 Android 的 `device.getInfo` |
| Android | 25 | 不包含 Windows 专属窗口命令 |
| 全集 | 29 | 宿主按平台矩阵过滤 |

`ping` 与 `capabilities.query` 是最小通路。其余命令覆盖文件、对话框、剪贴板、通知、
权限、事件、窗口、下载、外部链接和设备信息。

## 平台矩阵

| 命令组 | Windows | Android | 权限 |
| --- | --- | --- | --- |
| `ping`、`capabilities.query` | 支持 | 支持 | 无 |
| `app.getInfo`、`app.exit` | 支持 | 支持 | 无 / `window.control` |
| `fs.readText`、`fs.writeText`、`fs.exists`、`fs.remove` | 支持 | 支持 | `fs.read` / `fs.write` / `fs.remove` |
| `fs.readDir`、`fs.mkdir`、`fs.stat` | 支持 | 支持 | `fs.read` / `fs.write` |
| `dialog.message`、`dialog.confirm`、`dialog.openFile`、`dialog.saveFile` | 支持 | 支持 | `dialog` |
| `clipboard.readText`、`clipboard.writeText` | 支持 | 支持 | `clipboard.read` / `clipboard.write` |
| `notification.show` | 支持 | 支持 | `notification.show` |
| `permission.request` | 支持 | 支持 | 无 |
| `event.subscribe`、`event.unsubscribe` | 支持 | 支持 | 无 |
| `window.setTitle`、`window.minimize`、`window.maximize`、`window.setSize` | 支持 | 不支持 | `window.control` |
| `window.close` | 支持 | 支持 | `window.control` |
| `shell.openExternal` | 支持 | 支持 | `shell.open` |
| `http.download` | 支持 | 支持 | `http.download` |
| `device.getInfo` | 不支持 | 支持 | `device.info` |

`capabilities.query` 返回宿主实际实现的命令列表和当前可用权限。Android 对不支持的窗口命令
返回 `NOT_SUPPORTED`；Windows 对 `device.getInfo` 返回 `NOT_SUPPORTED`。

## 参数与生成

`commands.json` 中的 `args` 和 `result` 使用 JSON Schema 子集描述。当前生成器支持：

- `type`、`properties`、`required`、`additionalProperties`
- `items`、`enum`、`const`
- `minLength`、`maxBytes`
- 整数参数的类型和边界校验

运行 `python tools/codegen.py` 后，三端会得到同一份命令、事件、权限和参数校验器。不要在
宿主或前端手写平行的命令清单。修改协议后先运行 `python tools/validate_protocol.py`。

## 文件与事件语义

`fs.readText` 返回 `string` 或 `{ "file": "app://tmp/read-<id>.txt" }`。超过 64KiB 的结果
写入临时文件；再次读取该临时句柄时返回同一路径。

`app://data`、`app://cache`、`app://tmp` 可读写；`app://res` 与 `app://assets` 只读；
`app://picked/<id>` 表示用户选择的文件。宿主不向前端暴露真实路径或 `content://` URI。

`webview.reloaded` 是系统事件，页面重载后由宿主主动推送，不需要订阅；`window.resized`
通过 `event.subscribe` 订阅，尺寸单位为物理内容区像素。

## 新增命令

### `http.download`

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | string | 是 | 只允许 `https://` |
| `path` | string | 是 | `app://data`、`app://cache` 或 `app://tmp` 下的文件 |
| `overwrite` | boolean | 否 | 默认 `false` |

返回 `{ path, bytes }`。最多 5 次 HTTPS 重定向、120 秒总超时、100 MiB 上限。目标已存在且
未允许覆盖时返回 `IO_ERROR`；非法 URL 或路径返回 `INVALID_ARGS` 或 `PERMISSION_DENIED`。

### `shell.openExternal`

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | string | 是 | 绝对 `http` / `https` URL |

返回 `{ opened }`。宿主构造时配置精确 origin 白名单，默认空白名单；未授权 origin 返回
`PERMISSION_DENIED`。

### `device.getInfo`

仅 Android 可用，返回 `platform`、`osVersion`、`model`、`manufacturer`、`appVersion`、
`language`、`screenWidth`、`screenHeight`、`density`、`deviceName`、`timezone` 和
`architecture`。不返回任何持久设备标识。

### `window.setSize`

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `width` | integer | 是 | 逻辑外框宽度，最小 320 |
| `height` | integer | 是 | 逻辑外框高度，最小 240 |

仅 Windows 可用，返回 `{ updated }`。按当前 DPI 转成物理像素并夹取到工作区；最大化、
最小化或全屏状态返回 `NOT_SUPPORTED`。

## 调用示例

```ts
KSM.initialize();
const capabilities = await KSM.get();

const result = await KSM.http.download({
  url: 'https://example.com/file.bin',
  path: 'app://data/file.bin'
});

await KSM.shell.openExternal({ url: 'https://example.com/docs' });
```

标准响应：

```json
{
  "type": "response",
  "id": "req-1",
  "ok": true,
  "data": {
    "path": "app://data/file.bin",
    "bytes": 1024
  }
}
```

每个请求只返回一次响应。错误码必须属于 `errors.json`。

## 排查路径

1. 确认命令已写入 `commands.json`，并运行协议校验和代码生成。
2. 确认命令名没有添加 `KSM` 前缀，且参数结构符合 schema。
3. 确认宿主返回的 `id` 与请求一致，每个请求只返回一次。
4. 确认错误码属于 `errors.json`，平台不支持时返回 `NOT_SUPPORTED`。
5. 确认前端已初始化，并用 `KSM.get()` 读取能力缓存。
6. 确认超过 64KiB 的文本返回 `{ file }`，且句柄仍指向 `app://tmp/`。
7. 确认 `webview.reloaded` 由宿主系统事件触发，`window.resized` 已订阅。
8. 确认下载和外部链接经过 URL、路径和 origin 白名单校验。
