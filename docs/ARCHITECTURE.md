# 主应用架构与能力接口

本文描述当前主体源码及通用插件适配层，不定义具体插件内部实现。功能及版本边界以 [FEATURES](FEATURES.md) 为准；2026-10-16 的验证证据单独保存在 [审核记录](REVIEW.md)，不代表后续提交已完成相同验证。

## 分层与所有权

```text
Main.qml / Sidebar.qml：顶部栏、页面、弹层、命中区域
  ├─ 内置 QML 卡片 ── C++ 业务 QObject ── WinRT / Win32 / Core Audio
  ├─ DesktopCardCanvas ── UiCardManager：布局、碰撞、编辑事务、持久化
  ├─ TopIslandHost ── IslandBridge：持续活动、提醒队列、内置动作路由
  └─ ToolboxPage ── PluginRegistry ── PluginProcess / JSON IPC
                                            └─ 可信独立插件进程与独立窗口
```

主体为 Windows x64、C++20、Qt Quick/QML 应用。启动入口 [main.cpp](../src/main.cpp) 持有服务、上下文和 QML 引擎；QML 引擎先于其依赖服务销毁。编译源与库依赖由 [CMakeLists](../CMakeLists.txt) 列明；包权限由 [manifest](../packaging/AppxManifest.xml) 定义。

| 层 | 职责 | 明确不负责 |
| --- | --- | --- |
| QML 页面/卡片 | 组件树、业务绑定、样式、动效、交互及输入命中 | 随意执行外部插件提供的 UI 代码 |
| `UiCard` / `UiCardManager` | 原生卡片元数据、尺寸/位置、开启/固定、约束、碰撞、草稿和保存 | 系统业务、QML 动态代码、插件传输 |
| 业务服务 | 系统数据采集与操作、状态和异步生命周期 | 对外稳定 SDK 或任意 Agent 工具执行 |
| `IslandBridge` | 一项最高活动、提醒缓冲、去重、展示请求与来源信号 | Shell、系统权限授予、可靠任务调度 |
| `PluginRegistry` | 本地包/版本/信任/路由管理，每 ID 一个受管进程槽位 | 插件内部命令、终端渲染、第三方代码沙盒 |
| `PluginProcess` / `PluginProtocol` | 进程生命周期、本地握手、消息验证、展示数据适配 | 任意 RPC、共享宿主 QObject、运行插件 QML/DLL |

主体不编译、创建或注册终端专属类，不捆绑终端 Runner。插件窗口和插件自己的数据属于独立包；主体只保留通用管理与受限数据接口。

## 内部能力接口

内置 QML 通过 root context 的 `Q_PROPERTY`、`Q_INVOKABLE`、signals/slots 使用 C++ 服务；这是可信内部接口，不是版本化公共 API，不能直接交给外部模型。

| 接口组 | 入口与动作 |
| --- | --- |
| 界面与布局 | [DropdownController](../include/dropdowncontroller.h)：边缘/快捷键/命中区域；[UiCardManager](../include/uicardmanager.h)：`registerCard`、`configureDesktop`、`beginDesktopEdit`、`setDesktopGeometry`、`setEnabled`、`setPinned`、`saveEdit/cancelEdit` |
| 系统与音频 | [SystemTrayManager](../include/systemtraymanager.h)：主/会话音量与输出设备；[AudioInputManager](../include/audioinputmanager.h)：默认输入控制与峰值；[MediaMonitor](../include/mediamonitor.h)：会话、能力核验、控制和 seek |
| 网络与设备 | [WifiManager](../include/wifimanager.h)、[RadioManager](../include/radiomanager.h)、[BluetoothManager](../include/bluetoothmanager.h)：发现与受约束动作；[NetworkDetails](../include/networkdetails.h)：只读；[UsbManager](../include/usbmanager.h)：安全枚举/弹出 |
| 用户数据 | [NotificationListener](../include/notificationlistener.h) 与 [ViewModel](../include/notificationviewmodel.h)：系统同步/确认与筛选；[ClipboardManager](../include/clipboardmanager.h)：历史及删除结果；[ClipboardOcr](../include/clipboardocr.h)：手动本地识别 |
| 持续任务 | [FocusTimer](../include/focustimer.h)、[AwakeManager](../include/awakemanager.h)、[HotspotManager](../include/hotspotmanager.h)：独立生命周期和状态，不由灵动岛代替执行 |

## 灵动岛契约

[IslandBridge](../include/islandbridge.h) 接受 `publishState(sourceId,state,priority)`、`pushEvent(sourceId,key,state,priority,durationMs,validForMs)`、`withdraw`；输出 `activeState`、`queuedCount`、`eventActive`，发出 `displayRequested` 和来源详情信号。

持续活动的当前内置排序为：音频输入 60、媒体播放 40、专注 20、OCR 15、保持唤醒 10、暂停媒体 8、暂停专注 5。通知、完成及失败为短时事件；具体来源适配由 [IslandSources](../qml/IslandSources.qml) 和 [main.cpp](../src/main.cpp) 持有，插件不伪装内置来源。

[队列实现](../src/islandbridge.cpp) 上限为 32 个持续来源、16 条事件、64 条最近去重记录；同优先级先到者优先。正在显示事件至少展示 1.2 秒，之后更高且达到 95 的事件可抢占，较高持续活动也可让它暂退；被抢占事件保留剩余时长。队满可丢低优先级，不是可靠事件存储。

[TopIslandHost](../qml/TopIslandHost.qml) 负责单击展开内联控制、来源按钮、双击卡片开合、状态/尺寸动效和系统减少动画偏好。内置动作按来源、会话与能力检查；事件展示不证明任务仍在运行。

## 通用插件接口

### 包、版本和信任

[PluginRegistry](../include/pluginregistry.h) 仅接收本地独立进程包：`schemaVersion=2`、`apiVersion=1`、`runtime=process`。清单包含 `id/name/version/minimumHostVersion/entry/files/pages`；`permissions` 可缺省，仅用于展示声明，不强制系统权限。版本为三或四段非负整数，文件列表及 SHA-256 必须完整，通过路径、链接、额外文件、大小和宿主兼容检查。[包校验实现](../src/pluginregistry.cpp) 是最终规范。

工具箱是唯一插件管理 GUI。`importTrustedFolder/reload/trustAndEnable/setPluginEnabled/launch/stop/selectVersion/openRoute` 不提供通用系统命令。发现、导入和启用不自动运行；同版本不可覆盖。可保存多个版本，但每 ID 只选择一版；运行中切换须确认，旧进程退出后才保存选择，不自动启动新版本。关闭请求超过 2 秒可终止受管插件进程，可能丢失其未保存内容。选择版本丢失/篡改时不自动回退。

包保存于应用本地 `trusted-plugins/<id>@<version>/`；旧平铺目录只兼容读取。信任摘要按版本、启用偏好与选择记录由 QSettings 保存。哈希是完整性与本地信任绑定，不是发行者签名；插件仍是当前用户权限的可信进程。没有任意第三方代码隔离、卸载、在线升级或签名分发体系。

### 固定 JSON 消息

[PluginProcess](../src/pluginprocess.cpp) 每次启动创建仅同用户访问的本地管道、随机令牌，继承系统环境并增加 `SIDEBAR_PLUGIN_ENDPOINT/TOKEN/ID`。插件以换行分隔 UTF-8 JSON 发送；8 秒内完成 `hello(protocolVersion,pluginId,token)`，宿主返回 `ready` 及 `task.result.v1` 能力。

| 方向 | 消息 | 宿主行为 |
| --- | --- | --- |
| 插件 → 主体 | `state` / `event` | 接收有限纯文本、图标名、色调、进度、已声明 `pageId`；宿主添加来源命名空间和路由，交给灵动岛 |
| 插件 → 主体 | `result`，`schemaVersion=1` | 接收任务事实；宿主生成标题、耗时、退出码、图标与颜色，转换为短时提醒 |
| 插件 → 主体 | `withdraw` / `closing` | 撤回来源 / 正常退出协商；断线、停用和进程退出也撤回展示 |
| 主体 → 插件 | `ready` / `show` / `close` / `theme` | 能力/有限颜色令牌、独立窗口生命周期；不暴露宿主对象 |

`result` 的固定正文为 `pageId/label/outcome/exitCode/elapsedMs`，外层包含 `source/key/priority`。`outcome` 仅允许 `success/failed/cancelled/timeout/crashed/start-failed`；成功退出码必须为 0、失败必须非 0；取消、超时和启动失败不把哨兵值显示成真实退出码。结果只是插件报告，宿主不独立证明插件执行结果。

[PluginProtocol](../include/pluginprotocol.h) 拒绝未知消息/字段、无效类型/范围与跨插件路由。持续优先级 0–60、事件/结果 0–95；每插件最多 8 个来源、每秒 30 条消息、单帧 32 KiB。没有 HTML/UI 代码字段，展示文本按 PlainText 渲染；不支持 QML、脚本、任意命令、自定义动作或动态卡片组件注册。插件可以传展示事实，不能据此获得主体的系统操作权限。

## 资源、数据与设计原则

- 主体服务和 QML 共用一个进程；媒体/音频采集与部分系统读取另有工作线程。插件是独立进程，不能把其内存记作主体占用。[MonitorScheduler](../src/monitorscheduler.cpp) 管理监控周期；页面隐藏不等于所有后台线程停止。
- 频谱与峰值按可见媒体/输入来源启用；通知模块停用撤销监听。其他模块的后台生命周期并未形成统一资源预算，没有逐模块 CPU/内存或长期功耗承诺。
- 异步操作使用忙状态、操作代次和取消/超时过滤旧结果。取消等待不承诺 Windows 操作回滚；不能以 `busy=false` 代替实际系统状态。
- 系统是操作结果权威：通知/剪贴板删除按确认结果更新，USB 不强制释放占用，设备操作重新核验。危险或破坏性动作需目标清晰、用户确认，不隐式扩大权限。
- 用户布局、设置和业务职责分离。布局保存是事务；固定/关闭是显示偏好，不偷偷变成权限或服务开关。QML 定义 UI，C++ 管理约束和系统能力。
- 图片 OCR、音频峰值和频谱不实现云上传或音频保存；仍需防止插件主动发送敏感文本。主设置/固定 ID 不是用户内容备份。
- 主体不承载具体插件内容，不加载插件 UI 代码，不把内部 QObject 透传成 Agent SDK。独立进程和管道认证不是安全沙盒，也未消除全部同用户篡改及检查/执行竞态。
- 构建产物、签名私钥、个人日志和实机截图不属于最小源码交付；当前审核测试结果、未验证项和发行状态分别记录，不以旧结果替代新验收。
