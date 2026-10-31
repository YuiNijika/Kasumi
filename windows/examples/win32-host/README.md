# Kasumi Win32 + WebView2 示例

> 运行一个真实 Win32 窗口，通过已安装的 Kasumi SDK 加载 `https://app.local/index.html` 并调用宿主服务。

## 模块边界

这个工程是独立消费者，不把 `windows/src/` 加入自己的源文件列表，也不复制路由实现。

| 文件 | 责任 |
| --- | --- |
| `CMakeLists.txt` | `find_package(Kasumi CONFIG REQUIRED)`、链接静态库、默认编译内嵌 Web 资源 |
| `main.cpp` | Win32 窗口、数据目录、内嵌资源提供器和 WebView2 生命周期 |
| `../../../examples/showcase/` | Windows 与 Android 共用的完整能力页面 |

构建前必须先安装 SDK。消费者只使用公共头文件 `<kasumi/...>` 和导入目标
`Kasumi::kasumi_windows_bridge`。

## 构建

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

默认构建把 `examples/showcase/` 编译为 EXE 内的 Win32 `RCDATA`，输出目录不包含
`web/`。WebView2 使用系统默认用户数据目录，宿主应用数据仍写入
`%LOCALAPPDATA%\Kasumi\Example`，不会创建项目内的 `KasumiData/`。

示例要求 Evergreen WebView2 Runtime。缺失时显示安装提示并退出，不内置 Fixed Version
Runtime。

## 可选配置

外置 Web 资源模式会在构建后复制 `web/`，适合需要直接替换页面文件的调试流程：

```powershell
cmake -S windows/examples/win32-host `
  -B windows/examples/win32-host/build-external `
  -DKASUMI_EXAMPLE_EXTERNAL_WEB=ON `
  -DCMAKE_PREFIX_PATH="$PWD/windows/build/sdk/install" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build windows/examples/win32-host/build-external --config Release
```

运行时也可以覆盖默认路径：

| 参数 | 作用 |
| --- | --- |
| `--web-root <dir>` | 使用指定目录作为外置 Web 根目录 |
| `--user-data-folder <dir>` | 使用指定 WebView2 用户数据目录 |
| `--url <url>` | 覆盖启动页面，开发模式使用 `http://localhost:5173/index.html` |
| `--dev` | 开启 WebView DevTools，仅供开发宿主使用 |

不传 `--user-data-folder` 时，普通启动使用 WebView2 系统默认 profile。`--smoke-test`
只为自动化测试使用独立临时 profile。
使用 `python cli/ksm.py build --platform windows --target example --external-web` 或
`package` 的同一参数，可以直接让命令行构建选择外置模式。
`pnpm ksm dev` 会启动 `examples/showcase` 静态服务器，并以 `--url` 和 `--dev` 启动该宿主。

## 验证点

```powershell
python cli/ksm.py test --platform windows --target example
```

CTest 使用 `--smoke-test` 隐藏启动窗口，加载页面后执行 `ping`，再通过 `app.exit` 退出。
该测试要求机器已安装 Evergreen WebView2 Runtime。

手工验证：

1. 生产地址必须是 `https://app.local/index.html`，开发地址只能是
   `http://localhost:5173/index.html`，不能直接使用 `file://`。
2. 页面消息来源必须匹配 `https://app.local`，近似域名和端口变体必须被拒绝。
3. 运行连接检查、文件往返、窗口尺寸、下载和外部链接，活动记录应显示结果。
4. 最大化或最小化窗口后调整尺寸，宿主应返回 `NOT_SUPPORTED`。
5. 下载失败、超时或取消后，目标目录不应留下 `.part` 文件。

## 排查

1. `find_package(Kasumi)` 失败：检查 `CMAKE_PREFIX_PATH`。
2. 启动提示缺少 WebView2 Runtime：安装 Evergreen Runtime 后重新运行。
3. 页面空白：默认模式检查 EXE 是否包含内嵌资源；外置模式检查 `web/index.html` 是否与 EXE 同级。
4. 请求一直超时：检查 WebView2 消息回调是否回到 UI 线程，以及虚拟主机是否设置成功。
5. 外部链接被拒绝：检查构造 `KSMWebView2Host` 时传入的 origin 白名单。
6. 下载被拒绝：确认 URL 为 HTTPS、目标是可写虚拟路径，且目标不存在或显式允许覆盖。
