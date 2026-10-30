# Kasumi 示例

> 展示应用如何以外部消费者身份使用已构建的 Kasumi SDK，而不是把框架源码复制进应用工程。

| 目录 | 用途 | 消费方式 |
| --- | --- | --- |
| `showcase/` | Windows 与 Android 共用的完整能力页面 | 静态 Web 资源 |
| `android-host/` | 独立 Android 应用工程 | 本地 Maven 中的 AAR |

Windows 示例位于 `windows/examples/win32-host/`，通过 `find_package(Kasumi CONFIG REQUIRED)`
消费已安装的静态库和公共头文件。

## 构建顺序

1. 先构建 SDK。
2. 再构建独立示例工程。
3. 示例只依赖安装目录、AAR 或 Maven 坐标，不直接编译框架实现文件。

完整命令见 `examples/showcase/README.md`、`examples/android-host/README.md` 和
`windows/examples/win32-host/README.md`。
