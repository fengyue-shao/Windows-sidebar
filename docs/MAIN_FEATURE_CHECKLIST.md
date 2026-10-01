# 主应用功能循环清单

范围经用户确认：剪贴板图片 OCR、网络详情、移动热点、定时保持唤醒、USB 外接存储设备安全弹出。AI 层仍为插件，不并入核心。不推送远程。

起始检查点：`09bdab0`（codex-do）。每项按接口核验 → 实现 → 构建/自动测试 → 安全的实际验证 → 更新此清单循环。需要用户授权的真实系统操作不自动执行。五小时额度剩余 5% 后只输出文档。

| 项目 | 进度 | 验证与边界 |
| --- | --- | --- |
| 剪贴板图片 OCR | 第一版已实现；真实引擎测试通过 | MSIX 下生成测试图成功识别；手动历史图片识别需用户试用，不自动截图/上传/覆盖剪贴板 |
| 网络详情 | 已实现；只读快照测试通过 | 连接、计费、适配器、IPv4/IPv6、网关、DNS、MTU、链路速率；连接等级不保证网站可达 |
| 移动热点 | 第一版已实现；校验与状态读取测试通过 | 需要 wiFiControl；配置及开关二次确认；真实开关未执行，不记录密码 |
| 定时保持唤醒 | 已实现；模拟系统请求测试通过 | 用户明确要求只做模拟测试；包括到期、失败、重试、替换、退出释放；不阻止主动睡眠 |
| USB 安全弹出 | 第一版已实现；保护规则与枚举测试通过 | 物理 USB 存储函数校验、16 种磁盘保护组合、未知 ID 拒绝；当前没有可验收设备，未实际弹出 |

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
