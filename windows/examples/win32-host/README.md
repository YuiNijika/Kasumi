# Kasumi Win32 + WebView2 Host

> 运行一个真实的 Win32 窗口，通过 WebView2 加载 `https://app.local/index.html` 并调用 Kasumi 宿主服务。

## 模块边界

这个示例负责展示宿主装配方式，不复制协议路由：

| 文件 | 责任 |
| --- | --- |
| `main.cpp` | Win32 窗口、数据目录、文件系统和 WebView2 生命周期 |
| `../../src/platform/ksm_webview2_host.h` | WebView2 创建、虚拟主机映射、消息和窗口事件适配 |
| `web/` | 带 CSP 的示例页面和前端调用代码 |

示例不会内置 Fixed Version WebView2 Runtime。启动时如果找不到 Evergreen Runtime，会显示安装提示并退出。

## 运行

构建后保持 `web/` 目录与 EXE 同级。宿主数据写入
`%LOCALAPPDATA%\Kasumi\Example`，WebView2 用户数据写入其中的 `WebView2` 子目录。

```powershell
cmake -S windows -B windows/build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build windows/build --config Release
.\windows\build\Release\kasumi_win32_example.exe
```

CTest 使用 `--smoke-test` 启动隐藏窗口，页面加载后执行 `ping` 并通过 `app.exit` 退出。
该测试要求机器已安装 Evergreen WebView2 Runtime。

示例允许 `https://example.com` 作为外部链接 origin；下载只接受 HTTPS，并写入
`app://data`、`app://cache` 或 `app://tmp`。

## 验证点

1. 窗口地址必须是 `https://app.local/index.html`，不能直接使用 `file://`。
2. 页面消息来源必须匹配 `https://app.local`，近似域名和端口变体必须被拒绝。
3. 点击 Ping、写入笔记、读取笔记、调整窗口和外部链接按钮，活动列表应显示结果。
4. 最大化或最小化窗口后调用窗口尺寸，宿主应返回 `NOT_SUPPORTED`。
5. 下载失败、超时或取消后，目标目录不应留下 `.part` 文件。

## 排查

1. 启动提示缺少 WebView2 Runtime：安装 Evergreen Runtime 后重新运行。
2. 页面空白：确认 `web/index.html` 与 EXE 在同一输出目录。
3. 请求一直超时：检查 WebView2 消息回调是否回到 UI 线程，以及 `app.local` 虚拟主机是否设置成功。
4. 外部链接被拒绝：检查构造 `KSMWebView2Host` 时传入的 origin 白名单。
5. 下载被拒绝：确认 URL 为 HTTPS、目标是可写虚拟路径，且目标不存在或显式允许覆盖。
