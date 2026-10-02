# 系统接口稳定性加固（2026-10-02）

## 范围与边界

用户反馈系统持续异常，Dell 助手正在下载驱动。本次只修改应用代码、编译并运行纯模拟测试，不安装、不启动主应用，不扫描真实无线网络、不切换无线设备/热点、不设置真实保持唤醒、不弹出 USB。尚无证据将整机异常归因于本应用。

## 本次改动

| 模块 | 加固内容 |
| --- | --- |
| USB | 区分 ERROR_NO_MORE_ITEMS 与枚举错误；接口详情失败/变化时丢弃整个结果，禁止用不完整列表弹出；设备信息集由 RAII 释放。仍只使用安全弹出 API，不强制卸载或绕过占用拒绝。 |
| 媒体 | 异步操作使用一次 wait_for + GetResults，单次等待上限 3 秒，超时尝试取消；退出先请求线程中断，后续等待不再执行；封面流限制 12 MB，避免大小截断及异常分配。 |
| 网络详情 | 适配器枚举移至后台线程，同一时间只保留一个查询，结果排队回到 UI；功能测试改为等待异步结果。 |
| Wi-Fi | 注册 ACM 扫描完成/失败通知，4.5 秒兜底超时；保留扫描句柄至完成；回调代际隔离；通知回调内不关闭 WLAN 句柄；连接使用列表中对应适配器，不再默认第一个适配器。 |
| 无线开关 | 串行请求、每种设备只保留最后一个待执行目标；检查 SetStateAsync 返回值，短期重读状态；访问失败保留上次状态并提示；处理中禁用 GUI 开关。 |
| 热点 | 保留活动热点的控制对象，上游连接消失后优先使用原对象读状态/提交关闭；启动及配置仍要求可用连接。 |

## 验证结果

- Release 全量编译成功（退出码 0），包含主应用和测试目标。
- `sidebar-stability-test` 退出码 0：用模拟操作覆盖成功、错误、取消、超时、退出中断；检查只等待一次、超时不读取结果、中断不进入等待。
- `git diff --check` 通过。
- 原有实机媒体、音频捕获、MSIX、OCR、热点、USB 测试本次未运行。用户之前确认 OCR 与热点通过，但不等同于本轮修改后的验收。

## 尚未证明的事项

- Dell 更新结束后需人工复核无线扫描/切换、热点上游断开后的关闭、媒体会话失效与应用退出，以及 USB 占用拒绝提示。
- Win32 同步调用及驱动/服务调用本身仍可能阻塞。后台查询避免正常 UI 被枚举阻塞，不代表为底层调用提供硬超时；网络线程退出仍等待正在进行的查询。
- WinRT 取消为尽力请求，不能保证系统操作被撤销；媒体单次异步等待有上限，不保证所有同步 COM 调用或整个退出流程严格在 3 秒内结束。
- 热点、无线及其他 fire-and-forget 任务仍需进一步统一取消、超时和应用退出生命周期处理；本次没有将其改为完整任务管理系统。
- 不能依据本次编译和模拟测试宣称整机异常已解决，仍需 Dell 检查结果及异常时间附近的 Windows 诊断证据。

## 核验依据

- [USB 枚举的正常结束与错误](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdienumdeviceinterfaces)
- [Windows 安全弹出与拒绝原因](https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_request_device_ejectw)
- [适配器枚举的同步调用与缓冲策略](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses)
- [WLAN 扫描完成通知与超时](https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanscan)
- [无线开关许可与最终状态](https://learn.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.setstateasync)
- [C++/WinRT 单一等待与超时](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/concurrency-2)

当前保持 codex-do 分支，不推送远程，不替换安装版本。
