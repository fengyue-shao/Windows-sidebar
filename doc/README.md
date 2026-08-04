# Windows Sidebar 项目文档

本文档对应当前 `0.7.1` 版本，用于帮助开发者快速理解项目结构、运行机制和扩展方式。

## 项目简介

Windows Sidebar 是一个使用 Qt 6、QML、C++20 和 Windows Runtime 编写的桌面侧边栏。应用固定在屏幕右侧，主要提供：

- CPU、内存、GPU、网络和磁盘分区监控；
- Windows 系统通知读取、展示和同步删除；
- Windows 全局媒体信息展示与播放控制；
- 白天、黑夜和跟随系统三种主题；
- 可折叠、始终置顶的无边框侧边栏界面。

## 文档导航

- [项目架构](architecture.md)：启动流程、线程模型、数据流和 C++/QML 连接方式。
- [模块说明](modules.md)：每个 C++ 与 QML 模块的职责和主要接口。
- [构建与部署](build-and-deploy.md)：开发环境、编译、MSIX 打包和测试机部署。
- [扩展指南](development.md)：添加监控项、页面、主题颜色和 Windows 功能的方法。

## 顶层目录

```text
Windows-sidebar/
├─ assets/                 QML 使用的图标资源
├─ doc/                    项目文档
├─ include/                C++ 头文件
├─ packaging/              MSIX 清单和应用图标
├─ qml/                    界面与交互组件
├─ scripts/                编译、打包和安装脚本
├─ src/                    C++ 实现
├─ build-and-deploy.cmd    一键编译部署入口
├─ CMakeLists.txt          CMake 构建配置
└─ README.md               仓库简要说明
```

## 快速定位

| 需求 | 首要查看文件 |
|---|---|
| 修改窗口大小、位置或折叠动画 | `qml/Main.qml` |
| 调整页面组合和底部导航 | `qml/Sidebar.qml`、`qml/SystemControlBar.qml` |
| 修改系统监控界面 | `qml/SystemMonitorPanel.qml`、`qml/DiskPanel.qml` |
| 修改媒体卡片 | `qml/MediaBar.qml`、`qml/MediaMarquee.qml` |
| 修改通知列表 | `qml/NotificationArea.qml`、`qml/NotificationItem.qml` |
| 添加或修改监控数据 | `include/*monitor.h`、`src/*monitor.cpp` |
| 修改刷新周期 | `src/main.cpp` 中的 `MonitorScheduler::addModule` |
| 修改主题颜色 | `include/thememanager.h`、`src/thememanager.cpp` |
| 修改 Windows 权限 | `packaging/AppxManifest.xml` |
| 修改打包过程 | `scripts/build-msix.ps1` |

## 重要运行条件

通知监听依赖 MSIX 提供的应用包身份。直接运行构建目录中的 `appqt_rainmeter.exe` 可以查看大部分界面和监控数据，但无法保证 Windows 通知接口正常工作。完整测试应安装 MSIX，并从 Windows 开始菜单启动应用。

