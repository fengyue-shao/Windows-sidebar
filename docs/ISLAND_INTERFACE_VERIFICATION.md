# 灵动岛接口核验与第一版

2026-10-01；本地预览，基于检查点 `1aa0353`，本轮修改尚未提交、未推送、不覆盖 MSIX。

## 已确认原则

持续活动仅选一个主显示；默认音频输入 60 > 媒体 40 > 专注 20。同级保留最早仍有效的活动，不因倒计时、进度、封面或音量采样更新抢占。任何应用的标准活动捕获会话均可触发输入状态，设备存在/未静音/瞬间峰值不是活动证据。

非持续事件统一排队。专注完成默认 90；实时新通知 80；终端插件运行 10、完成 80、失败 90，由插件逐次声明。数值是第一版可调整的默认值，不是操作系统紧急等级。

## 官方接口核验

- [Audio Sessions](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audio-sessions)：会话分为播放或捕获；输入识别枚举全部活动 `eCapture` 端点，不枚举 `eRender`，因此不会把本应用输出 FFT 回环当作麦克风输入。
- [AudioSessionState](https://learn.microsoft.com/en-us/windows/win32/api/audiosessiontypes/ne-audiosessiontypes-audiosessionstate)：Active 表示有流正在运行，即使没有说话也仍活动。观察者只读会话状态，不创建输入音频流。
- [GetSessionEnumerator](https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nf-audiopolicy-iaudiosessionmanager2-getsessionenumerator)：枚举可能不知道通知回调中的新会话，必须自己维护列表。本版订阅创建通知、枚举初始列表、在 MTA 工作线程维护会话引用，并约 500 ms 检查状态与端点变化。
- [RegisterSessionNotification](https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nf-audiopolicy-iaudiosessionmanager2-registersessionnotification)：注册后读取枚举器数量启用通知；退出/端点移除撤销订阅。
- [GetProcessId](https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nf-audiopolicy-iaudiosessioncontrol2-getprocessid)：`AUDCLNT_S_NO_SINGLE_PROCESS` 只给创建者 PID，不代表全部调用者。本版明确标注跨进程/未知来源，不推断真实应用。

进程名从有限权限的进程映像路径取得，只显示可执行文件名，不将完整路径传入 UI。读不到名称则显示未知应用/PID。浏览器仅识别进程，不识别网页；应用友好名称和代理映射未加入。多个应用显示数量汇总及名称列表，不选择未经用户确认的主应用。当前列表仅在内存，不写入使用历史、日志或网络。

此方案覆盖标准可观察捕获会话，不承诺独占模式、特殊驱动、受保护进程、系统代理、极短流与所有第三方程序的完整识别。无检测结果仅表示未发现标准活动会话，不表示系统绝对无人使用麦克风。

## 待回答问题

| 问题 | 选择 | 当前保守处理 |
|---|---|---|
| 浏览器名称 | 仅浏览器 / 进一步识别网页 | 仅进程名，不采集网页 |
| 跨进程、系统代理 | 保留未知标记 / 增加经过验证的映射 | 标注跨进程或未知，不猜测归属 |
| 多输入应用的主显示 | 汇总 / 最先 / 最近开始 / 用户指定 | 汇总，不擅自选主应用 |
| 应用使用记录 | 仅实时 / 内存最近历史 / 可清除的本地持久历史 | 仅当前列表，无持久化 |
| 插件紧急级权限 | 可自主声明 / 宿主限制等级 | 保留现有 -100 至 100 的声明范围，不新增权限隔离承诺 |
| 通知内容隐私 | 仅应用名 / 标题 / 正文预览 | 仅应用名及“新通知”，不展示正文 |

## API 与调度

```qml
islandBridge.publishState("plugin.example.running", {
    title: "任务运行中", subtitle: "处理中", iconName: "terminal", route: "terminal"
}, 10)
islandBridge.pushEvent("plugin.example", "job-123", {
    title: "任务完成", subtitle: "退出码 0", iconName: "terminal", route: "terminal"
}, 80, 4000, 15000)
islandBridge.withdraw("plugin.example.running")
islandBridge.withdraw("plugin.example")
```

`publishState` 更新不会重新取得入队顺序。最多 32 个持续来源、16 个事件；事件按优先级降序、同级先进先出。低于当前持续活动等级的事件等待至符合条件或过期。当前事件正常显示到结束，后来的普通事件不打断；95+ 且高于当前事件等级可在最短 1.2 秒后抢占，原事件保留剩余时长与原有效期。

显示时长 1–10 秒，等待有效期 1–60 秒。从首次实际展示开始计显示时间。相同 source/key 在队列中合并内容，但不延长首次有效期/当前展示时长；展示完成的同一 key 在 15 秒内去重。完成事件应使用独立任务 key，连续音量等操作可复用一个 key。缓存有上限，不接受无限积压；接口返回 bool 表示是否接收。

`publish` 旧 API 保留：TTL=0 映射到持续状态，TTL>0 映射为 legacy key 的事件；不再表示每次更新抢占。`withdraw` 取消指定来源的持续状态及其未完成事件。

新主活动或开始展示事件时发出 `displayRequested`，高频更新不重复请求。完全隐藏时窗口壳层进入 IslandOnly，只浮出独立黑色胶囊，不显示完整状态栏、不抢焦点、不自动展开卡片。事件期间暂停自动隐藏，结束后恢复已有自动隐藏规则；并非持续活动永久锁住顶部栏。IslandOnly 无剩余活动时立即隐藏；边缘/快捷键唤出会恢复完整精简栏。单击按白名单 route 打开对应页面，双击展开/收起；采用 Qt 官方互斥单/双击信号，遵循系统双击间隔。

通知监听新增 `notificationArrived`，只在实时变化回调发出；初始历史同步仍仅更新通知列表。终端新增结构化 `commandCompleted(exitCode, elapsedMs)`，不解析中文状态文本。插件卸载撤回自身状态；异常关闭/取消命令当前撤回运行状态，尚未统一为完成事件。

当前仍是受信任的同进程 QML 插件接口，不提供插件命名空间所有权/优先级权限隔离。

## 验证

最终 Release 构建、调度器测试与全部页面 UI 冒烟通过。输入观察者实测成功注册 4 个活动输入端点，当时活动输入应用数为 0；没有开启麦克风流来制造测试。新增更高优先级持续活动时，当前较低优先级事件经过最短展示保护后退回缓冲，主显示切回持续活动；被中断事件保留原有效期，避免过期后重放。

`sidebar-island-test`：唯一主活动、稳定更新、不反复唤出、同级顺序、事件缓冲/合并、过期、恢复、去重、显示请求，以及本机输入端点枚举和会话订阅。不打开麦克风流。此次实测无活动输入应用；实际第三方录音、通话开始/结束识别、热拔插和跨进程来源需后续场景验证。

UI 冒烟包含 Hidden → 事件请求 → IslandOnly，确保不展开 Cards、胶囊外区域不拦截点击；继续回归全部页面、插件、媒体和命中区域。测试夹具不代表真实通知授权或真实麦克风使用场景。
