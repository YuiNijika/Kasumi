# Kasumi 开发入口

> 说明协议、前端桥接、Windows 宿主和 Android 宿主的开发顺序与排查入口。

## 当前进度

已完成的基础能力：

- `protocol/commands.json` 使用 JSON Schema 描述全部命令参数和返回值，三端生成命令、事件、权限和参数校验器。
- 前端 `KSM` 根入口提供缓存的 `get()` / `capabilities()`，事件订阅自动跟随页面生命周期。
- Windows 与 Android 的文件命令支持 `app://data`、`app://cache`、`app://tmp` 以及 `app://picked/<id>` 外部文件句柄。
- 文件读写限制为 16MB，单条消息限制为 64KB。
- Android WebView 已启用 CSP、导航拦截、`window.open` 拦截、生产环境关闭调试，并将命令分发限制为最多 8 条并发。
- Windows 消息宿主使用固定工作线程池，超过并发上限返回 `BUSY`，排队请求支持取消。
- 契约测试入口已拆分为 Windows C++ runner 与 Android JVM 测试，CI 会执行双端测试。

仍需在真实 WebView2 宿主中完成的部分：

- 将 `KSMWebViewHost::NotifyWebViewReloaded`、`NotifyWindowResized` 接到实际窗口生命周期。
- 将 WebView2 的导航、`window.open`、CSP 与消息来源校验接到真实宿主窗口。
- 运行 Windows C++ 编译、Android Gradle 构建和双端契约测试，确认当前环境之外的构建结果。

## 模块边界

| 模块 | 入口 | 负责内容 |
| --- | --- | --- |
| 协议 | `protocol/commands.json` | 命令、事件、权限和平台能力 |
| 代码生成 | `tools/codegen.py` | 生成三端契约常量 |
| 前端 | `frontend/src/index.ts` | `KSM` 根入口和 API 封装 |
| Windows | `windows/src/bridge/ksm_router.h` | WebView2 消息路由 |
| Android | `android/app/src/main/.../KSMRouter.kt` | WebView 消息路由 |
| 测试 | `tests/contract/runner.ts` | 双端契约行为 |

## 开发顺序

1. 修改 `protocol/commands.json` 和权限契约。
2. 运行 `ksm codegen` 或 `python tools/codegen.py`。
3. 实现 Windows 和 Android 命令处理器。
4. 在 `frontend/src/api/` 增加前端封装。
5. 增加 `tests/contract/cases/` 用例。
6. 执行前端类型检查和双端构建。

## 常用命令

```bash
python cli/ksm.py codegen
python cli/ksm.py build
pnpm --dir frontend run typecheck
```

## 安全边界

- 前端只使用 `app://` 虚拟路径。
- 文件选择器返回 `app://picked/<id>`，不向前端暴露真实路径或 `content://` URI。
- 宿主必须先拒绝危险路径，再进行规范化和前缀检查。
- 业务代码不得直接访问 WebView 注入对象。
- 单条 JSON 消息不能超过 64KB。
- 单文件读写不能超过 16MB。
- Android 不得使用 `addJavascriptInterface`。
- Android WebView 必须保持 CSP、导航拦截和 `window.open` 拦截。

## 排查路径

1. 确认命令已经写入 `protocol/commands.json`。
2. 运行 `python tools/codegen.py`，确认生成文件包含该命令、权限和参数校验器。
3. 确认 `capabilities.query` 返回平台能力。
4. 确认请求经过参数和权限校验。
5. 确认响应 `id` 与请求一致。
6. 确认 WebView 回调运行在要求的线程。
