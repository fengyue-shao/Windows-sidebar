# 主应用功能循环清单

范围经用户确认：剪贴板图片 OCR、网络详情、移动热点、定时保持唤醒、USB 外接存储设备安全弹出。AI 层仍为插件，不并入核心。不推送远程。

起始检查点：`09bdab0`（codex-do）。每项按接口核验 → 实现 → 构建/自动测试 → 安全的实际验证 → 更新此清单循环。需要用户授权的真实系统操作不自动执行。五小时额度剩余 5% 后只输出文档。

| 项目 | 进度 | 验证与边界 |
| --- | --- | --- |
| 剪贴板图片 OCR | 第一版用户确认通过；完成提醒已接入灵动岛 | 新改动仍待实机回归；不自动截图/上传/覆盖剪贴板，不在灵动岛展示识别正文 |
| 网络详情 | 已实现；只读快照测试通过 | 连接、计费、适配器、IPv4/IPv6、网关、DNS、MTU、链路速率；连接等级不保证网站可达 |
| 移动热点 | 第一版用户确认通过；异常控制路径已加固 | 需要 wiFiControl；配置及开关二次确认；新改动待回归，不记录密码；Agent 未执行真实开关 |
| 定时保持唤醒 | 已实现；模拟系统请求测试通过 | 用户明确要求只做模拟测试；包括到期、失败、重试、替换、退出释放；不阻止主动睡眠 |
| USB 安全弹出 | 已实现；枚举失败保护加固，结果接入灵动岛 | 用户报告资源管理器打开时弹出失败；完整拒绝码尚未取得，仍待人工验收，不绕过占用保护 |

## 官方接口依据

- [Windows.Media.Ocr（桌面应用需要包身份）](https://learn.microsoft.com/en-us/uwp/api/windows.media.ocr)
- [ConnectionProfile](https://learn.microsoft.com/en-us/uwp/api/windows.networking.connectivity.connectionprofile)
- [NetworkOperatorTetheringManager](https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager)
- [SetThreadExecutionState](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate)
- [CM_Request_Device_EjectW](https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_request_device_ejectw)

## 验收记录

2026-10-01：Release 构建、普通 EXE 功能测试、卡片展开态和 OCR 编辑区 UI 自检均通过；自检同时回归 Hidden/Bar/Cards/IslandOnly、输入区域、媒体会话与频谱策略。修复原生 TextArea 样式警告、OCR 结果复制后编辑内容重置风险、自检截图过早的尺寸警告；将样式和截图警告加入失败条件。

已获用户许可安装本地 MSIX 测试版及使用项目开发证书，最终安装版本为 `0.8.1.1`。Windows 拒绝不同内容的同版本 `0.8.1.0` 覆盖（0x80073CFB），因此提升修订号正常更新，未卸载用户应用或删除数据。真实包身份下生成图片 OCR 测试通过；系统报告热点可用、当前关闭。保持唤醒只使用模拟接口；热点真实开关和 USB 真正移除尚未执行。USB 当前可核验设备数为 0。

最终包身份日志：`work/ui-tests/feature-packaged-02.log`；普通 EXE 日志：`work/ui-tests/feature-unpackaged-final.log`；最终 UI 截图：`work/ui-tests/features-release.png` 及 `.features.png` / `.ocr.png` 后缀输出。上述相对路径均以仓库父目录工作区为基准。

补充安装版 UI 验收：`test-packaged-ui.ps1` 通过 Windows 原生进程句柄确认退出码 0，要求五张全新截图存在且非空；结果 `PACKAGED_UI_TEST PASS`。对应日志 `work/ui-tests/feature-packaged-04.log`，截图 `work/ui-tests/packaged-ui-verified.png` 及后缀输出。初次补测的 PowerShell Process 包装器退出码为空，不能作为失败/成功依据；已替换为 OpenProcess/WaitForSingleObject/GetExitCodeProcess 核验，未把空值当作通过。

日志位于工作区 `work/ui-tests/`，不写入私人图片、OCR 正文、热点密码或网络地址。测试截图使用合成网络/USB 数据和合成 OCR 文本；监控截图可能包含当前媒体及设备名称，不对外上传。

这是一轮实现与安全自动验收，不等于全部真实设备场景验收。普通 EXE 的 OCR/热点身份限制、设备能力差异、实际 Wi-Fi 共享和 USB 占用拒绝需要按[使用与验收说明](MAIN_FEATURE_USAGE.md)继续验证。

## 第二轮证据补充与完成审计

最终本地安装版本更新为 **0.8.1.2**。`feature-ocr-edges-packaged.log` 记录真实包身份下五类 OCR 场景通过：文字图、空白图、损坏数据、超过引擎最大边长、超过 12 MB。复制逻辑使用模拟写入接口验证编辑文字被传递、空/超长结果拒绝、状态更新不重置编辑结果；没有覆盖真实剪贴板。`ocr-edges-packaged-ui.png` 及四个后缀输出验证安装版 UI 原生退出码 0，报告 `PACKAGED_UI_TEST PASS`。

| 原目标要求 | 当前权威证据 | 审计结论 |
| --- | --- | --- |
| 先提交、在 codex-do 开发、不推送 | 初始 09bdab0，后续本地提交；分支/工作区检查 | 已落实 |
| 图片历史 OCR | 生产解码/识别管线五种合成图测试、编辑区截图、模拟复制 | 引擎与边界已验证；用户选中历史原图的完整操作链仍待人工试用 |
| 网络连接详情 | WinRT 默认连接 + GetAdaptersAddresses 只读快照；实际活动适配器字段与 UI 布局检查 | 当前连接读取通过；断网/VPN 多场景未覆盖 |
| 移动热点 | 包权限通过打包校验；安装版系统报告 supported=1、已关闭；输入校验通过 | 实际保存配置、开启/关闭及另一设备连接仍待许可和测试 |
| 定时保持唤醒 | 模拟系统请求和时钟验证范围、到期、失败、重试、替换、退出释放 | 符合用户“仅模拟测试”要求；不执行真实电源测试 |
| USB 存储安全弹出 | 存储函数过滤、16 种保护组合、未知 ID 拒绝、真实只读枚举 | 没有可核验设备；正向枚举、占用拒绝和真实弹出仍无硬件证据 |
| 不擅自修改系统与额度限制 | 测试接口隔离；真实操作许可问题未回答；额度检查远未到 5% 剩余阈值 | 未进行热点开关、保持唤醒或真实 USB 弹出 |

结论：不能把自动测试通过当作所有实机场景已验收。剩余进展需要用户允许热点真实测试、提供非系统 USB 测试设备，或用户按使用说明自行验收；不为通过测试而强制改变系统状态。长任务尚未宣告整体完成。

## 最新状态（2026-10-02，优先于上述历史记录）

用户已手动确认第一版 OCR、热点通过。USB 占用失败仍未完成原因核验；系统随后出现异常，用户重启并使用 Dell 助手更新驱动。驱动更新期间停止实机接口测试，只编译、运行模拟测试及纯数据离屏 UI 验证。不将历史的“待试用”误报为用户从未验收，也不将旧版验收视为新改动已通过。

稳定性改动见 [接口加固记录](STABILITY_REVIEW_2026-10-02.md)。基础功能及灵动岛最新进度见 [本轮记录](CORE_ISLAND_PROGRESS_2026-10-02.md)。插件系统暂停开发；不替换安装版本，不推送远程。需要人工验收时停止，告知用户，不持续重复测试循环。
