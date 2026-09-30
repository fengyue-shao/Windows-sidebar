# 主应用规划与 Windows API 核验

日期：2026-10-01。状态：规划，不代表功能已实现。

## 产品边界

主应用服务 Windows 日常操作，降低使用门槛。AI、语音及 Agent 工具暴露暂归插件系统，不进入核心依赖。工具箱是用户管理插件的唯一 GUI 入口，设置只管理主应用行为。受限终端保持可选，不宣称强安全沙盒。

## 功能优先级

| 阶段 | 功能 | 官方接口与边界 |
| --- | --- | --- |
| 优先 | 麦克风输入设备、音量、静音 | Win32 Core Audio IAudioEndpointVolume；不是系统麦克风隐私权限开关 |
| 优先 | 媒体会话切换、播放进度与跳转 | WinRT GlobalSystemMediaTransportControlsSessionManager.GetSessions、会话时间线；按播放器支持的控制能力显示按钮 |
| 优先 | 剪贴板图片 OCR，再扩展截图 OCR | Windows.Media.Ocr.OcrEngine，需要包身份、可用语言，并检查图像尺寸上限；桌面截图采用 GraphicsCapture HWND 互操作 |
| 后续 | 网络连接详情 | NetworkInformation / ConnectionProfile；连接状态不能保证具体服务可达 |
| 后续 | 移动热点 | NetworkOperatorTetheringManager，wiFiControl 能力及硬件检查；失败需解释原因 |
| 后续 | 电源状态、定时保持唤醒 | PowerManager；保持唤醒使用 Win32 SetThreadExecutionState，不覆盖用户主动睡眠 |
| 后续 | USB 安全弹出 | Win32 CM_Request_Device_EjectW；显示拒绝原因，不能强制拔除 |

## 核验结论与兼容性债务

- Qt packagedClassicApp MSIX 不等于 UWP。BrightnessOverride 不支持桌面应用，不能因为有包身份就假设可用。
- UserNotificationListener 可读取、监听、删除/清空通知，不提供通用的第三方通知回复或原始操作激活接口；先做分类、搜索和阅读体验。
- 音频默认输出切换现有实现使用手工声明的 IPolicyConfig，需作为兼容性债务保留记录，不能宣称完全采用公开 WinRT。
- 插件共享宿主 QML 引擎，当前权限声明不等于强制权限隔离；AI 插件开放系统操作前必须先建立明确授权、审计及取消机制。

## 官方参考

- [Core Audio 音量接口](https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nn-endpointvolume-iaudioendpointvolume)
- [媒体会话](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssession)
- [OCR](https://learn.microsoft.com/en-us/uwp/api/windows.media.ocr.ocrengine)
- [桌面窗口截图互操作](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow)
- [网络信息](https://learn.microsoft.com/en-us/uwp/api/windows.networking.connectivity.networkinformation)
- [移动热点](https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager)
- [电源状态](https://learn.microsoft.com/en-us/uwp/api/windows.system.power.powermanager)
- [保持唤醒](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate)
- [设备安全弹出](https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_request_device_ejectw)
- [桌面应用 WinRT 支持边界](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/winrt-api-desktop-app-support)
- [通知监听](https://learn.microsoft.com/en-us/uwp/api/windows.ui.notifications.management.usernotificationlistener)

## 当前顺序

先完成顶部下拉 UI 设计审核，再实现壳层和回归测试；其后按上表逐项增加核心能力。AI 层只预留插件边界，不提前实现。
