# 主应用功能与版本

2026-10-16 审核候选。本文与 [架构与接口](ARCHITECTURE.md) 描述当前主体源码；不把开发功能、历史验收和已安装软件混为一谈。本轮构建及测试结果以 [审核交付记录](REVIEW.md) 为准，本文不声明本轮测试已通过。

## 版本边界

| 对象 | 版本及含义 |
| --- | --- |
| 主体源码 / MSIX 清单 | `0.8.2.0`，见 [CMake](../CMakeLists.txt) 与 [manifest](../packaging/AppxManifest.xml) |
| 主体运行时展示 | `0.8.2.0-preview`，见 [入口](../src/main.cpp) |
| 本轮 EXE 版本标识 | `FileVersion=0.8.2.0`、`ProductVersion=0.8.2.0-preview`，由 [版本资源模板](../packaging/version.rc.in) 从 CMake 项目版本生成；标记为预发布 |
| 本机已安装应用 | 2026-10-16 只读核验为 WindowsSidebar `0.8.2.0`；不是本审核源码重新构建的证明 |
| 未发布主体增量 | 自由桌面卡片、多版本插件选择、结构化任务结果协议等，已存在于源码，未因本次整理自动安装 |
| 独立终端插件 | `0.8.3.1` 可选预览，位于主体仓库之外，不计入主体版本或内置功能 |

同一个 `0.8.2.0` 标签不能证明安装版和当前工作区内容相同。审核后若发布当前增量，应先确定新版本，再构建、签名和安装；本次不以旧版本号覆盖发行新内容。

## 功能清单

“已实现”表示当前源码提供能力，不等于所有设备和实机场景均已验收。

| 部分 | 当前能力 | 边界 / 源码入口 |
| --- | --- | --- |
| 顶部下拉界面 | 顶边停留唤出精简栏；展开监控、通知、专注、剪贴板、工具箱；自动隐藏、月历 | 快捷键冲突保留原值；全屏抑制为可选项。[Main](../qml/Main.qml)、[Sidebar](../qml/Sidebar.qml)、[DropdownController](../include/dropdowncontroller.h) |
| 外观与显示器 | 主题、透明度、宽度、紧凑模式、屏幕选择、模块开关 | 不修改系统分辨率；关闭显示模块不统一等于停止其所有后台服务。[UiSettings](../include/uisettings.h)、[DisplayManager](../include/displaymanager.h) |
| 系统监控 | CPU、内存、GPU、上下行速度、磁盘容量 | 只读统计，不是进程管理器或专业性能分析器。[MonitorScheduler](../include/monitorscheduler.h)、[DropdownMonitor](../qml/DropdownMonitor.qml) |
| 输出音频 | 主音量/静音、每应用会话混音、默认输出选择 | 输出切换使用 `IPolicyConfig` 兼容接口，有非公开接口维护风险。[SystemTrayManager](../src/systemtraymanager.cpp) |
| 输入音频 | 默认输入名称、音量/静音、输入/输出峰值条、输入设备列表和系统设置入口 | 不在应用内切换默认输入；峰值不录音。[AudioInputManager](../include/audioinputmanager.h) |
| 媒体 | 系统媒体会话选择、播放控制、封面、元数据、可用时间线拖动、动态频谱 | 仅控制 Windows 暴露的会话；频谱是默认输出系统混音，不是所选媒体独占音轨。[MediaMonitor](../include/mediamonitor.h)、[MediaSpectrum](../include/mediaspectrum.h) |
| Wi-Fi / 无线开关 | 扫描、信号/安全状态、已存配置连接、开放/WPA2 新建连接、隐藏网络、断开、忘记；Wi-Fi/蓝牙开关 | 其他新建安全类型引导系统设置；忘记当前连接需确认并先核验断开。[WifiManager](../include/wifimanager.h)、[RadioManager](../include/radiomanager.h) |
| 蓝牙 | 30 秒动态发现、立即更新列表、停止保留列表、配对/取消配对 | 配对等待上限 45 秒；不是通用 GATT、音频连接或文件传输接口。[BluetoothManager](../include/bluetoothmanager.h) |
| 网络详情 | 连接等级、计费、适配器、IP、网关、DNS、MTU、链路速度 | 只读快照；系统连通等级不保证某网站可达。[NetworkDetails](../include/networkdetails.h) |
| 移动热点 | 系统能力与状态读取、配置、开关、取消等待 | 依赖包身份/能力及设备支持；45 秒超时或取消不保证系统操作回滚，不自动重试。[HotspotManager](../include/hotspotmanager.h) |
| 通知 | 按应用分组、文字搜索、仅未读、单项/当前筛选批量操作、系统确认后移除 | 需通知访问权限；本地已读不等于系统全局已读；关闭模块停止读取、保留列表，重开同步。[NotificationViewModel](../include/notificationviewmodel.h)、[NotificationListener](../src/notificationlistener.cpp) |
| 剪贴板 | Windows 历史文本/图片预览、恢复内容、单项删除、清除非固定项、应用内固定标记 | 固定仅保护本应用清理，不是 Windows pin 或永久收藏；系统未保存的文件项不能凭空恢复。[ClipboardManager](../include/clipboardmanager.h) |
| 图片 OCR | 手动识别选中历史图片、本机语言引擎、结果编辑/复制、取消等待 | 需包身份及已装 OCR 语言；30 秒等待上限；不自动截图、上传或覆盖剪贴板。[ClipboardOcr](../include/clipboardocr.h) |
| 专注 / 保持唤醒 | 专注计时、暂停恢复、节奏预设与完成统计；1–240 分钟系统/屏幕唤醒请求 | 不修改电源计划、不阻止主动睡眠、不恢复上次唤醒任务；真实保持唤醒仅有模拟验收。[FocusTimer](../include/focustimer.h)、[AwakeManager](../include/awakemanager.h) |
| USB 存储 | 枚举、目标二次核验、确认后安全弹出、完整系统拒绝提示 | 排除系统/启动/未知磁盘；不强制解锁或关闭文件；无热插拔自动刷新、专用取消/等待超时。[UsbManager](../include/usbmanager.h) |
| 灵动岛 | 一个当前最高持续活动、有界提醒队列、隐藏时主动展示、内联快捷控制、来源跳转、动效与减少动画适配 | 只是展示聚合及内置动作白名单，不是系统任务执行器。[IslandBridge](../include/islandbridge.h)、[TopIslandHost](../qml/TopIslandHost.qml) |
| 工具箱 / 通用插件 | 可信本地包导入、完整性校验、版本选择、显式信任、启停/打开、固定 JSON 状态与结果展示 | 不含具体插件功能、QML/DLL 注入、任意命令 RPC、自动更新或卸载流程。见 [插件接口](ARCHITECTURE.md#通用插件接口) |

通知、OCR、热点的包身份与能力声明见 [manifest](../packaging/AppxManifest.xml)。直接运行开发 EXE 不能代替 MSIX 场景验收；系统授权、驱动和设备能力也可能限制可用性。

## 自由桌面卡片：可选开发预览

启动主体 EXE 时使用：

```text
appqt_rainmeter.exe --native-card-preview --preview-cards
```

`--native-card-preview` 切换监控页画布；`--preview-cards` 使预览保持展开，不能用它证明正常自动隐藏已验收。不加前一参数仍为默认下拉界面。

目前只有九张原生监控卡片：实时监控、麦克风、磁盘、网络详情、USB、系统控制、热点、保持唤醒、媒体。通知、专注、剪贴板及插件 UI 未迁移到自由布局。

- 在当前屏幕可用范围拖动、自由拉伸；禁止越界/重叠，无效操作保留上一个合法位置，不挤走其他卡片。
- 编辑时顶部显示 `enabled`（开启）与 `pinned`（固定）。关闭不停止业务服务，可在“卡片”列表重新开启；固定卡片在顶部栏隐藏后仍原位显示，不等于锁定位置。
- 保存/取消/恢复默认为编辑事务；未保存时阻止直接隐藏、换页或退出。空间不足保留开启偏好，但不堆叠放置。
- 布局保存到应用本地 `cards/layouts-v2.json`；只读兼容旧 v1 文件。`--card-layout-file=<绝对路径>` 可指定隔离审核文件。

分工见 [UiCard](../include/uicard.h)、[UiCardManager](../include/uicardmanager.h)、[DesktopCardCanvas](../qml/DesktopCardCanvas.qml)、[NativeMonitorCards](../qml/NativeMonitorCards.qml)。管理器不描述 QML 组件树，卡片关闭也不等价于模块停用。

## 验收边界

仓库保留逻辑、生产 QML 及包身份自检源码；本次运行结果由审核交付记录补写，不沿用历史 PASS 冒充本次测试。

历史限定实机证据包括顶部栏/页面、可信插件导入，以及 F500 USB 的目录打开弹出、文件占用拒绝和释放后恢复。以下仍需按具体环境验收：真实 Wi-Fi 切换、蓝牙配对、热点第二设备、非空通知/剪贴板操作链、媒体/麦克风来源、多版本插件切换、多屏/DPI、长时间资源使用。保持唤醒维持仅模拟测试；不能宣称全功能、全设备无 bug。
