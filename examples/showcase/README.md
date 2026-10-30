# Kasumi 基础能力示例

> 用一个静态页面覆盖 Kasumi 的文件、对话框、剪贴板、通知、窗口、网络、设备与事件能力。

## 模块边界

这个目录是 Windows 与 Android 示例宿主共用的 Web 资源。它只调用公开消息契约，
不包含 C++ 或 Kotlin 框架源码，也不绕过宿主校验。

| 文件         | 责任                                                          |
| ------------ | ------------------------------------------------------------- |
| `index.html` | 无内联脚本和样式的完整能力面板，带严格 CSP                    |
| `styles.css` | 响应式布局、键盘焦点、触控尺寸和减少动画偏好                  |
| `app.js`     | WebView2 与 Android WebMessage 双通道、超时、取消和能力过滤   |

页面通过 `capabilities.query` 读取当前平台的真实命令清单。Android 上的窗口标题、
最小化、最大化、尺寸调整会禁用；Windows 上的设备信息会标记为 Android 专属。
禁用只改善提示，最终权限和平台校验仍由宿主完成。

## 构建与运行

Windows 和 Android 都先构建 SDK，再由独立示例工程消费产物。示例不会把框架源码
复制到应用工程中。

### Windows

```powershell
python cli/ksm.py build --platform windows --target sdk
cmake -S windows/examples/win32-host `
  -B windows/examples/win32-host/build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="$PWD/windows/build/sdk/install" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build windows/examples/win32-host/build --config Release
.\windows\examples\win32-host\build\Release\kasumi_win32_example.exe
```

CMake 会把本目录复制为示例 EXE 同级的 `web/`。页面加载地址是
`https://app.local/index.html`。

### Android

```bash
python cli/ksm.py build --platform android --target sdk
gradle -p examples/android-host assembleDebug
```

SDK 会先发布到 `android/kasumi/build/repo`，独立示例再从该 Maven 仓库解析
`dev.kasumi:kasumi:0.1.0`。示例的 `assets.srcDir` 直接指向本目录。

## 能力覆盖

| 分区   | 命令                                                                                         |
| ------ | -------------------------------------------------------------------------------------------- |
| 概览   | `ping`、`capabilities.query`、`app.getInfo`、`app.exit`                                      |
| 文件   | `fs.mkdir`、`fs.writeText`、`fs.exists`、`fs.readText`、`fs.stat`、`fs.readDir`、`fs.remove` |
| 对话框 | `dialog.message`、`dialog.confirm`、`dialog.openFile`、`dialog.saveFile`                     |
| 剪贴板 | `clipboard.writeText`、`clipboard.readText`                                                  |
| 通知   | `permission.request`、`notification.show`                                                    |
| 窗口   | `window.setTitle`、`window.setSize`、`window.minimize`、`window.maximize`、`window.close`    |
| 网络   | `http.download`、`shell.openExternal`                                                        |
| 设备   | `device.getInfo`                                                                             |
| 事件   | `event.subscribe`、`event.unsubscribe`、`window.resized`、`webview.reloaded`                 |

`?smoke=1` 只执行 `ping` 和 `app.exit`，供 Windows 示例宿主做无人工交互的启动检查。

## 安全边界

- 下载 URL 固定为 `https://example.com/`，目标只使用可写虚拟根。
- 外部链接只打开宿主白名单中的 `https://example.com/`。
- 文件选择器只返回虚拟句柄，页面不接触真实文件系统路径。
- 设备信息不读取 Android ID、序列号、IMEI、广告 ID 或其他持久标识。
- 页面不使用内联脚本、内联样式或远程资源。

## 排查路径

1. 页面提示未连接：确认宿主创建了 WebView，并加载 `https://app.local/index.html`。
2. 请求全部超时：检查消息是否为字符串，以及响应 `id` 是否与请求一致。
3. Android 无法调用：确认 `window.ksm` 存在且来源为 `https://app.local`。
4. 文件操作被拒绝：检查路径是否位于 `app://data`、`app://cache` 或 `app://tmp`。
5. 下载返回 `IO_ERROR`：检查目标是否存在，以及 `overwrite` 是否显式开启。
6. 外部链接返回 `PERMISSION_DENIED`：检查宿主构造函数中的 origin 白名单。
7. 通知不显示：先请求 `notification.show` 权限，再检查系统通知设置。
