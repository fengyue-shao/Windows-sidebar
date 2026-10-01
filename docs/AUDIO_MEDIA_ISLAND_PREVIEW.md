# 音频、媒体与顶部扩展接口预览

2026-10-01，基于 `codex-do` 检查点 `9abd9df`，版本仍为 0.8.1.0-preview。本地预览，不推送远程、不覆盖安装版。

## 麦克风与峰值条

监控页的麦克风独立卡片控制默认输入设备（eCapture / eConsole），提供静音与 0–100 输入音量。每秒检查默认设备 ID，跟随 Windows 设置变更和设备拔插；无设备/无法控制时禁用操作并显示状态。不批量静音其他设备。

设备切换入口目前打开 Windows 声音设置 `ms-settings:sound`；输入设备枚举已保留在 `audioInputManager.devices`，下一版可据此扩展原生选择界面。未新增未公开的默认输入设备切换接口。

`AudioInputManager` 读取 Core Audio `IAudioMeterInformation`，输入和输出分别显示真实峰值，不生成随机动画。监控页可见时 100 ms 采样，隐藏/切换页后停止快速采样；设备状态仍每秒检查。只读取归一化峰值，不创建音频捕获客户端、不保存采样、不上传声音。麦克风没有活跃录音流时可能是零；仅有软件峰值计的独占模式也可能报告零。

音量条表示信号峰值，不是当前音量百分比，也不是频谱分析。静音时视觉条归零，设备不支持计量时显示不可用。输出控制同步检测默认端点变化，设备选择按 ID 而非同名匹配。

视觉修订：根据用户参考图，移除独立的横向电平格，改为卡片背景内低对比度、垂直居中的 25 根起伏音频柱。每 100 ms 保留一个真实峰值，显示最近约 2.5 秒的历史；柱高使用平方根映射，使较弱的信号可见，仍不是各频段的 FFT。无信号回到短线，静音/失去接口/隐藏时清空历史。不使用随机柱高，不占布局行，不添加输入处理器。

官方依据：[Peak Meters](https://learn.microsoft.com/en-us/windows/win32/coreaudio/peak-meters)、[IAudioEndpointVolume](https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nn-endpointvolume-iaudioendpointvolume)。

## 媒体会话与时间线

使用 WinRT GSMTC 枚举已公开的媒体会话；“自动”跟随系统当前会话，点击会话标签后锁定本应用控制对象。会话结束时回到自动选择；只改变本应用的控制目标，不修改系统当前会话。后台工作线程负责读取与控制，UI 不等待 WinRT 调用。

进度每 500 ms 刷新；播放时依据时间线快照时间戳和播放速率推算当前位置并限制到曲目范围。只在播放器声明允许定位且提供有效可定位范围时启用拖动；否则显示只读进度或“未提供时间线”。拖动释放后请求定位，按 MinSeekTime/MaxSeekTime 钳制；拖动期间切换会话/曲目则丢弃旧操作。播放器拒绝操作会显示提示。

媒体卡片保留此前的横向渐变、约 120 px 自适应封面、播放状态遮罩、MEDIA 来源/标题/艺术家/专辑层次及播放按钮比例。会话选择整合在上方，进度整合在底部；不再沿用上一版简化的小封面平面卡片。回归测试继续覆盖会话选择、定位与旧会话保护，新增峰值历史长度和静音清空断言。

官方依据：[GSMTC Session](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssession?view=winrt-26100)、[Playback Controls](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionplaybackcontrols?view=winrt-26100)、[Timeline Properties](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessiontimelineproperties?view=winrt-26100)。

## 媒体背景频谱

媒体卡片新增独立 `FFT` 开关，默认关闭、不持久化。媒体卡片只保留在监控页，通知页不再包含媒体卡片或媒体空状态占位。2026-10-01 更新：打开开关后，模块启用、所选媒体播放、窗口可见且媒体卡片或媒体灵动岛可见时开始分析；收起卡片或离开监控页但媒体灵动岛仍可见时继续。暂停、完全隐藏、关闭模块或 FFT，或者两处均不可见时停用并清空柱子。卡片和灵动岛共享一个采集器。

独立 MTA 工作线程通过 WASAPI 共享模式回环读取默认输出设备的系统混音，不是所选 GSMTC 会话的独立音轨。其他应用声音会影响柱高；输出到其他端点的播放器不在此源内。切换系统默认输出后每秒检测 ID 并重建客户端；失败时有状态提示和重试。受保护内容、独占播放及特殊驱动未实测。

2048 点 Hann 窗 FFT，32 个约 40 Hz–16 kHz 的对数频段（上限随采样率限制），柱高采用 -70–0 dB 的归一化显示，约 25 fps 更新、快上升/慢下降平滑，居中绘制在媒体背景。不是峰值历史条，也不是严格校准的响度计。立体声/多通道目前平均为单声道，反相通道可能相互抵消。

与仅查询输入/输出峰值不同，这个频谱功能确实创建**输出回环捕获客户端**并在内存内处理短音频缓冲；不打开麦克风、不保存文件、不上传、不记录原始 PCM 到日志。仅频段数值进入 UI。隐藏或停用后工作线程停止客户端并丢弃缓冲。

官方依据：[Loopback Recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)、[IAudioCaptureClient::GetBuffer](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer)。

`sidebar-spectrum-test` 验证无声归零、500 Hz 与 8 kHz 的频段区分、NaN/值域、真实默认端点回环初始化和 32 段输出、停止后清空。不播放测试音；纯频率测试在内存内生成样本。媒体 UI 截图采用明确标注的静态频段夹具，不是实际采集效果；第三方播放器实际播放下的频谱体验仍需用户审阅。

## 灵动岛预留

`TopIslandHost.qml` 位于顶部栏中央，采用不透明黑色胶囊、白色主文字、灰色摘要和浅蓝图标。时间在左侧，联网/音量/电量和设置在右侧，不被活动替换。空闲、持续活动、短时事件分别采用小胶囊/紧凑摘要/较宽摘要；中央预留固定 420 DIP 活动区，其他组件不随活动尺寸变化移动。窄窗口在同一个顶栏内分两行排布，避免重叠。完全隐藏时新活动仅唤出灵动岛，胶囊之外仍可点击穿透。双击展开/收起卡片；单击通过 `IslandBridge.detailsRequested(sourceId)` 按白名单路由到对应页面。专门的灵动岛动画与细粒度详情展开仍未制作。

未来模块可调用：

```qml
islandBridge.publish("plugin.example", {
    title: "任务进行中", subtitle: "正在处理", iconName: "timer", progress: 0.4
}, 20, 5000)
islandBridge.withdraw("plugin.example")
```

接口已扩展为持续状态与事件队列，同级状态不因更新抢占。最多 32 个持续来源、16 个事件；旧 `publish` 的正 TTL 现在映射为短暂事件。具体时长、去重、恢复、麦克风来源边界和待选问题见 [灵动岛接口核验](ISLAND_INTERFACE_VERIFICATION.md)。卸载插件须撤回其来源。该接口不是安全隔离层。

## 验证与边界

构建、模拟媒体会话选择/拖动/旧会话保护/只读定位、音量条 NaN/越界/静音状态，以及灵动岛优先级/撤回测试已通过。系统诊断确认默认输入控制和输入/输出峰值接口可用；桌面截图观察到两路真实峰值。实际双击收起与精简栏设置弹层已观察通过。未为测试改变用户麦克风静音或音量。

原生集成测试通过：两个无声音的桌面 SMTC 测试源，经过真实 Windows 媒体接口枚举、手动切换、分别定位到 35 / 75 秒，并回到自动跟随；输出 `MEDIA_INTEGRATION PASS two native sessions + selection + seek + follow`。测试不打开音频流、不注册持久化应用身份，退出后清理自建进程。测试脚本显式等待 Qt GUI 子系统进程，检查退出码和 PASS 日志，不依赖 PowerShell 的 `$LASTEXITCODE`。测试工具默认不编译、不安装到应用包。

新版实际桌面截图已检查麦克风卡片、两路电平和统一后的按钮/滑杆；模拟媒体截图也检查了会话标签与进度布局。具体第三方播放器对会话切换和 seek 的接受程度尚需实际播放条件下验证；受控 SMTC 测试不代表所有播放器兼容。未测试麦克风设备热拔插、独占音频、多显示器和不同 DPI；未新增录音测试流。包身份通知限制仍与之前一致。

```powershell
powershell.exe -NoProfile -File scripts/test-dropdown-ui.ps1 -Executable ../build-plugin-integration/Release/appqt_rainmeter.exe -CapturePath ../work/ui-tests/media-fixture.png

# 原生媒体集成测试需要先以 SIDEBAR_BUILD_MEDIA_TESTS=ON 配置并编译。
powershell.exe -NoProfile -File scripts/test-media-integration.ps1 -BuildDirectory ../build-plugin-integration/Release
```

CapturePath 所在目录需存在。模拟媒体渲染图与实际桌面截图只保存在工作区，不提交私人屏幕内容。
