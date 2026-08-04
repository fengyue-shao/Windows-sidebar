# 模块说明

## 系统监控模块

### `PeriodicModule`

周期模块的最小接口，只定义 `refresh()`。不要求继承 QObject，因此 `DiskMonitor` 可以直接同时继承 `QAbstractListModel` 和该接口。

### `MonitorBase`

CPU、内存、GPU 和网络监控的共同基类。`refresh()` 为最终实现，并转发到子类的 `updateData()`，使调度器不需要了解具体数据来源。

### `MonitorScheduler`

集中管理周期模块。`addModule(module, intervalMs)` 在编译期要求模块同时继承 QObject 和 `PeriodicModule`；运行时使用一个 QTimer 按不同周期分发刷新。

### `CpuMonitor`

读取 Windows 系统时间计数，通过相邻两次采样的 idle、kernel 和 user 差值计算 CPU 使用率，对 QML 暴露 `usage`。

### `MemoryMonitor`

读取 Windows 全局内存状态，计算物理内存使用百分比，对 QML 暴露 `usage`。

### `GpuMonitor`

通过 Windows PDH 的 GPU Engine 性能计数器采样 GPU 使用率。内部持有 PDH query/counter，并处理首次采样尚无有效差值的情况。

### `NetworkMonitor`

通过 Windows IP Helper 统计网卡收发字节。根据相邻采样差计算速度，并直接向 QML 提供格式化后的 `downloadSpeedText` 和 `uploadSpeedText`。

### `DiskMonitor`

既是周期模块，也是 `QAbstractListModel`。每 5 秒扫描可用磁盘分区，提供路径、名称、文件系统、总容量、已用容量、可用容量和使用率等角色。

对应界面：

- `SystemMonitorPanel.qml`：系统监控页面容器；
- `SystemMetricGauge.qml`：CPU、内存和 GPU 环形仪表；
- `DiskPanel.qml`：多分区列表；
- `WidgetCard.qml`：保留的通用进度卡片组件。

## 媒体模块

### `MediaWorker`

运行于独立线程，通过 `GlobalSystemMediaTransportControlsSessionManager` 获取当前系统媒体会话。

职责包括：

- 获取标题、艺术家、专辑和媒体提供者；
- 获取并转换封面图片；
- 监听媒体信息与播放状态变化；
- 执行播放/暂停、上一首和下一首命令。

当前只跟随 Windows 选定的当前媒体会话，不维护播放清单。

### `MediaMonitor`

主线程中的 QML 门面。接收 `MediaWorker` 的异步结果，并向 QML 暴露：

- `title`
- `artist`
- `album`
- `provider`
- `playing`
- `coverVersion`

`togglePlayPause()`、`next()` 和 `previous()` 将控制请求排队发送到媒体线程。

### `CoverImageProvider`

保存最新媒体封面，并作为 QML image provider 提供 `image://cover` 图片源。

对应界面：

- `MediaBar.qml`：媒体信息、封面和控制器；
- `MediaMarquee.qml`：超长标题、艺术家、专辑和来源的循环滚动显示。

## 通知模块

### `Notification`

通知数据结构，包含 ID、应用名、标题、内容、图标、图片、时间、未读状态、可删除状态和通知类型。

### `NotificationListener`

封装 WinRT `UserNotificationListener`：

- 请求通知访问权限；
- 首次启动加载现有 Toast 通知；
- 监听新增、更新和删除事件；
- 将 WinRT 通知转换为项目的 `Notification`；
- 将界面删除与清空操作同步到 Windows 通知中心。

该接口依赖 MSIX 应用包身份及 `userNotificationListener` capability。

### `NotificationModel`

QML 使用的列表模型。以通知 ID 去重，支持新增/更新、单项删除、清空、已读/未读切换和未读计数。

对应界面：

- `NotificationArea.qml`：列表标题、未读数、全部已读、清空和空状态；
- `NotificationItem.qml`：单条通知的显示和交互。

## 主题模块

### `ThemeManager`

应用颜色的唯一来源，支持：

- `FollowSystem`：跟随 Windows 配色；
- `Light`：固定白天模式；
- `Dark`：固定黑夜模式。

模式使用 Qt 设置持久化。QML 不应直接新增硬编码颜色；新增颜色语义时，应先在 `ThemeManager` 增加属性，再由组件绑定该属性。

## 顶层 QML 组件

| 文件 | 职责 |
|---|---|
| `Main.qml` | 窗口属性、屏幕右侧定位、展开/折叠宽度动画 |
| `Sidebar.qml` | 页面组合、当前分区状态、时间栏和控制栏布局 |
| `Timebar.qml` | 日期和时间显示 |
| `SystemControlBar.qml` | 系统/通知页面切换、主题菜单和退出功能 |
| `SystemMonitorPanel.qml` | 系统监控数据可视化 |
| `SystemMetricGauge.qml` | Canvas 环形占用率仪表 |
| `DiskPanel.qml` | 分区容量列表和进度条 |
| `MediaBar.qml` | 媒体封面、信息和播放控制 |
| `MediaMarquee.qml` | 长文本滚动 |
| `NotificationArea.qml` | 通知列表容器和批量操作 |
| `NotificationItem.qml` | 单条通知卡片 |

## 打包模块

### `AppxManifest.xml`

声明应用身份、版本、x64 架构、桌面应用入口、图标及以下能力：

- `runFullTrust`
- `globalMediaControl`
- `userNotificationListener`

### `build-msix.ps1`

创建 staging 目录、复制应用与资源、调用 `windeployqt`、调用 `makeappx`、创建或复用开发证书，并使用 `signtool` 签名。

### `install-msix.ps1`

按需将开发证书加入当前用户可信根证书库，然后使用 `Add-AppxPackage` 安装或更新应用。

### `build-and-deploy.ps1`

面向测试机的一次性入口，负责自动寻找 Qt、CMake、MSVC，完成 Release 配置、编译、打包和安装。

