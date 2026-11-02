# Kasumi protocol

> 说明协议契约的事实源、平台矩阵、参数校验、权限和跨端生成入口。

## 模块边界

`protocol/` 只定义跨端契约，不实现命令、执行权限或发送 WebView 消息。

| 文件                      | 责任                                        |
| ------------------------- | ------------------------------------------- |
| `commands.json`           | 命令、事件、参数、返回结构、权限和平台支持  |
| `commands.schema.json`    | 命令、事件和参数结构的 JSON Schema 校验规则 |
| `plugins.json`            | 内置插件清单、归属命令和插件版本            |
| `plugins.schema.json`     | 插件清单的 JSON Schema 校验规则             |
| `errors.json`             | 双端共用的错误码全集                        |
| `permissions.schema.json` | 权限声明                                    |
| `kasumi.config.schema.json` | 应用配置、包名、版本、命令 allowlist 和 Windows 标题栏开关 |
| `../version.json`         | 框架产品版本唯一事实源                      |
| `kasumi.version.schema.json` | 版本元数据 JSON Schema                   |

Windows、Android 和前端实现都必须以这些文件为准。协议字符串不加 `KSM` 前缀。

## 当前规模

`commands.json` 当前定义 **34 个命令和 2 个事件**，`plugins.json` 将全部命令
归入 **10 个内置插件**。产品版本由根目录 `version.json` 管理，SQL 插件的新增命令
使用 `since: "1.2.0"`。

| 平台    | 适用命令 | 说明                                 |
| ------- | -------: | ------------------------------------ |
| Windows |       33 | 不包含仅 Android 的 `device.getInfo` |
| Android |       30 | 不包含 Windows 专属窗口命令          |
| 全集    |       34 | 宿主按平台矩阵过滤                   |

`ping` 与 `capabilities.query` 是最小通路。其余命令覆盖文件、对话框、剪贴板、通知、
权限、事件、窗口、下载、外部链接、设备信息和数据库访问。

## 平台矩阵

| 命令组                                                                    | Windows | Android | 权限                                 |
| ------------------------------------------------------------------------- | ------- | ------- | ------------------------------------ |
| `ping`、`capabilities.query`                                              | 支持    | 支持    | 无                                   |
| `app.getInfo`、`app.exit`                                                 | 支持    | 支持    | 无 / `window.control`                |
| `fs.readText`、`fs.writeText`、`fs.exists`、`fs.remove`                   | 支持    | 支持    | `fs.read` / `fs.write` / `fs.remove` |
| `fs.readDir`、`fs.mkdir`、`fs.stat`                                       | 支持    | 支持    | `fs.read` / `fs.write`               |
| `dialog.message`、`dialog.confirm`、`dialog.openFile`、`dialog.saveFile`  | 支持    | 支持    | `dialog`                             |
| `clipboard.readText`、`clipboard.writeText`                               | 支持    | 支持    | `clipboard.read` / `clipboard.write` |
| `notification.show`                                                       | 支持    | 支持    | `notification.show`                  |
| `permission.request`                                                      | 支持    | 支持    | 无                                   |
| `event.subscribe`、`event.unsubscribe`                                    | 支持    | 支持    | 无                                   |
| `window.setTitle`、`window.minimize`、`window.maximize`、`window.setSize` | 支持    | 不支持  | `window.control`                     |
| `window.close`                                                            | 支持    | 支持    | `window.control`                     |
| `shell.openExternal`                                                      | 支持    | 支持    | `shell.open`                         |
| `http.download`                                                           | 支持    | 支持    | `http.download`                      |
| `device.getInfo`                                                          | 不支持  | 支持    | `device.info`                        |
| `sql.drivers`、`sql.open`、`sql.query`、`sql.execute`、`sql.close`        | 支持    | 支持    | `sql`                                |

`capabilities.query` 返回宿主实际实现的命令列表和当前可用权限。Android 对不支持的窗口命令
返回 `NOT_SUPPORTED`；Windows 对 `device.getInfo` 返回 `NOT_SUPPORTED`。

应用可在根目录 `kasumi.config.json` 中按平台选择启用命令。`commands: "all"` 会按本目录的
`commands.json` 平台矩阵展开全部命令；也可使用显式数组，此时 `ping` 与
`capabilities.query` 必填，未知命令、重复命令和跨平台命令直接拒绝。运行时的
`capabilities.query` 只返回配置后的命令，已知但未启用的命令返回 `NOT_SUPPORTED`。

Windows 可额外设置 `decorations` 布尔值，默认 `true`。设为 `false` 时宿主隐藏原生标题栏，
但保留窗口缩放和工作区行为；应用需自行提供标题栏控件。Android 不接受该字段。

## 插件清单

`plugins.json` 是插件归属的唯一事实源。每个协议命令只能属于一个插件，插件名使用
小写点分标识符，命令保持 `<domain>.<action>` 形式。每个插件同时声明
`frontendPackage`、`platforms`、`permissions` 和 `defaultPermissions`。校验器要求
权限与平台字段精确等于其命令集合的并集，避免清单和协议漂移。

| 插件 | 前端包 | 职责 | 命令数 |
| --- | --- | --- | ---: |
| `core` | `kasumi-bridge` | 应用生命周期、权限、事件和能力发现 | 7 |
| `fs` | `kasumi-plugin-fs` | 沙箱虚拟文件系统 | 7 |
| `dialog` | `kasumi-plugin-dialog` | 系统消息框、确认框和文件选择器 | 4 |
| `clipboard` | `kasumi-bridge` | 系统剪贴板文本 | 2 |
| `notification` | `kasumi-bridge` | 系统通知 | 1 |
| `window` | `kasumi-bridge` | 窗口标题、尺寸和生命周期 | 5 |
| `http` | `kasumi-bridge` | HTTPS 下载 | 1 |
| `shell` | `kasumi-bridge` | 白名单外部链接 | 1 |
| `device` | `kasumi-bridge` | 非持久设备信息 | 1 |
| `sql` | `kasumi-plugin-sql` | 数据库连接和参数化语句 | 5 |

Windows 的 `KSMRouter` 与 Android 的 `KSMRouter` 先派发插件，再进入仍由核心路由承载
的平台命令。宿主可以注册自定义 `KSMPlugin` 实现；`fs`、`dialog` 和 `sql` 可以用同 ID
插件显式替换，但替换实现必须完整声明该 ID 的全部生成命令。其他生成插件 ID 仍为保留值，
自定义插件也不得抢占任何生成命令。插件自己声明的命令会被加入 `capabilities.query`。
插件命令若不在 `commands.json` 中，由插件自己的参数校验器负责校验，不绕过宿主权限检查。

代码生成会输出 `KSM_PLUGIN_METADATA`、C++ `PluginMetadata()` 和 Kotlin
`pluginMetadata`。前端插件包只重新导出对应模块 API，原生权限、参数和来源校验仍在
Windows 或 Android 宿主执行。

## SQL 插件

SQL 插件通过 `sql.drivers` 暴露宿主实际注册的驱动。协议保留四个驱动标识：
`sqlite`、`mysql`、`postgres` 和 `mongodb`。标识存在不等于运行时驱动已安装，调用
`sql.open` 前必须先查询 `sql.drivers`。

当前内置实现：

| 驱动 | 状态 | URL 与行为 |
| --- | --- | --- |
| `sqlite` | Windows / Android 内置 | `:memory:` 或可写虚拟路径；禁止只读根、picked 路径和真实路径 |
| `mysql` | Windows 发现系统 ODBC 驱动后内置；Android 通过宿主 SPI 注册 | Windows 使用 `mysql://host[:port]/database?ssl-mode=required` 或 `disabled` |
| `postgres` | Windows 发现系统 ODBC 驱动后内置；Android 通过宿主 SPI 注册 | Windows 使用 `postgresql://host[:port]/database?sslmode=require` 或 `disable` |
| `mongodb` | 通过宿主驱动 SPI 注册 | 由注册驱动定义 URL、命令格式和文档映射 |

Windows 的远程驱动 URL 可带可选用户名和密码，但凭据不得出现在错误消息、日志或
`sql.drivers` 中。TLS 默认开启；只有显式查询参数才允许关闭。ODBC 路由要求系统已安装
对应厂商驱动，Kasumi SDK 不随包分发 MySQL、PostgreSQL 或 MongoDB 客户端。

SQL 参数只允许字符串、有限数值、布尔值和 `null`。`sql.query` 返回列名与行对象，
`sql.execute` 返回 `rowsAffected` 和可空的 `lastInsertId`。连接 ID 由宿主生成，
未知连接返回 `NOT_FOUND`，关闭连接后不得复用。

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

| 字段        | 类型    | 必填 | 说明                                                |
| ----------- | ------- | ---- | --------------------------------------------------- |
| `url`       | string  | 是   | 只允许 `https://`                                   |
| `path`      | string  | 是   | `app://data`、`app://cache` 或 `app://tmp` 下的文件 |
| `overwrite` | boolean | 否   | 默认 `false`                                        |

返回 `{ path, bytes }`。最多 5 次 HTTPS 重定向、120 秒总超时、100 MiB 上限。目标已存在且
未允许覆盖时返回 `IO_ERROR`；非法 URL 或路径返回 `INVALID_ARGS` 或 `PERMISSION_DENIED`。

### `shell.openExternal`

| 字段  | 类型   | 必填 | 说明                      |
| ----- | ------ | ---- | ------------------------- |
| `url` | string | 是   | 绝对 `http` / `https` URL |

返回 `{ opened }`。宿主构造时配置精确 origin 白名单，默认空白名单；未授权 origin 返回
`PERMISSION_DENIED`。

### `device.getInfo`

仅 Android 可用，返回 `platform`、`osVersion`、`model`、`manufacturer`、`appVersion`、
`language`、`screenWidth`、`screenHeight`、`density`、`deviceName`、`timezone` 和
`architecture`。不返回任何持久设备标识。

### `window.setSize`

| 字段     | 类型    | 必填 | 说明                   |
| -------- | ------- | ---- | ---------------------- |
| `width`  | integer | 是   | 逻辑外框宽度，最小 320 |
| `height` | integer | 是   | 逻辑外框高度，最小 240 |

仅 Windows 可用，返回 `{ updated }`。按当前 DPI 转成物理像素并夹取到工作区；最大化、
最小化或全屏状态返回 `NOT_SUPPORTED`。

## 调用示例

```ts
KSM.initialize();
const capabilities = await KSM.get();

const result = await KSM.http.download({
  url: "https://example.com/file.bin",
  path: "app://data/file.bin",
});

await KSM.shell.openExternal({ url: "https://example.com/docs" });
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
