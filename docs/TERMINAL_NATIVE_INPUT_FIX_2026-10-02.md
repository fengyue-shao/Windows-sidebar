# 原生终端输入修复与实测记录

日期：2026-10-02。分支：`codex-do`。基于 `0.8.1.3-preview`，本次未提升包版本、安装 MSIX、提交或推送远程。

## 问题与修复

用户复现：输入 `A`、`R`，原生终端显示成 `I`。

1. **隔离 Shell 与 Runner 的控制协议。** Runner 自身的标准输入承载 `I\t<Base64>` 等控制帧。创建 ConPTY 子进程时明确设置 `STARTF_USESTDHANDLES`，并将子进程三个标准句柄设为 NULL，避免 Shell 取得 Runner 的重定向标准句柄。终端输入由伪控制台提供，控制协议仅由 Runner 读取。
2. **保持异步启动期间的输入焦点。** Windows 上进程进入 Running 早于传输握手完成。旧 UI 在间隙切换到辅助输入框，恢复原生界面后隐藏框仍可能持有焦点。现在在启动动作之前记录待恢复焦点，握手后恢复正确的可见输入面；用户主动选择辅助输入或输出选择时不抢焦点，兼容模式回退时聚焦兼容输入框。
3. **统一 Win32 启动路径。** 将 Shell 路径与工作目录转为 Windows 原生分隔符后传给 `CreateProcessW`。真实 CMD 回归最初出现“无法创建目录或文件。”；加入此路径转换后正常到达提示符并通过后续测试。

相关官方资料：[Microsoft Terminal 维护者对 ConPTY 标准句柄的说明](https://github.com/microsoft/terminal/discussions/15814)、[Qt Windows QProcess 实现](https://raw.githubusercontent.com/qt/qtbase/v6.9.3/src/corelib/io/qprocess_win.cpp)、[Qt Item 可见性与键盘焦点](https://doc.qt.io/qt-6/qml-qtquick-item.html#visible-prop)。

## 自动验证

| 验证 | 结果 |
| --- | --- |
| Release 主应用与 Runner 构建 | 通过 |
| TerminalScreen 模型测试 | 退出码 0 |
| UI 回归：异步启动、独立 A/R、焦点归属、兼容回退、Tab、IME、粘贴与布局 | 通过，退出码 0 |
| 兼容管道：UTF-8、失败、超时、取消、重置、补全与日志隐私 | 通过，退出码 0 |
| 新增真实 CMD → ConPTY → Runner → 生产 TerminalManager 回归 | 通过，退出码 0 |
| Git 差异空白检查 | 通过 |

真实原生回归逐个发送 A/R，校验回显及协议帧不泄漏；随后验证退格、实际 echo 输出、中文 UTF-8、相同与变更尺寸、X 关闭帧正常退出和日志隐私。正常退出必须包含 Runner 退出码 0 与 NormalExit，不能把紧急强杀视为通过。残留检查直接核验测试 Runner 的 PID，未单独枚举 Shell 的所有后代。

新增测试目标：`sidebar-terminal-native-test`，在启用 `SIDEBAR_BUILD_TERMINAL_TESTS` 的构建中提供。默认参数为可写测试父目录；追加 `--interactive-preview <插件 Main.qml 路径>` 可打开五分钟自动关闭的实测窗口。测试运行环境需要 Qt 的 bin、platforms 和 QML 目录可发现；本次只为测试进程设置这些路径，未更改系统环境变量。

## 实际窗口按键验证

使用真实插件 QML、TerminalManager、Runner、ConPTY 与系统 CMD，未使用模拟终端。只替换预览颜色上下文并隔离数据目录。

- 实际按 Shift+A、Shift+R；当前中文输入法出现候选，分别提交后终端准确显示 `AR`，没有出现 `I`。
- 两次实际退格依次从 `AR` 变成 `A`、空提示符。
- 输入 `echo __UI_NATIVE_AR_OK__` 并实际按 Enter，得到正确输出和新的提示符。
- 点击预览关闭按钮；随后确认本次两个预览进程及已记录的 Runner 不再运行。

像素证据保存在工作区外层的 `work/screen-captures/`：`terminal-fixed-live-AR-committed.png`、`terminal-fixed-live-backspace-1.png`、`terminal-fixed-live-backspace-2.png`、`terminal-fixed-live-echo-output.png`。屏幕捕获与原生键盘输入技能用于实际输入验收，每次动作后读取截图；没有仅凭模拟输入返回值判定成功。

## 隔离与未验收边界

测试在随机临时目录内生成 CMD 配置、沙盒目录与日志，`LOCALAPPDATA` 只在测试进程中临时替换，结束时恢复并清理临时目录；不修改用户 Windows Terminal 配置。日志仅记录输入类型与字节数，不保存原文。

本次验证是**新构建的真实插件链路**，不是已安装主应用的完整验收。已安装 `0.8.1.3` 尚未替换。本地最新应用位于外层 `build-plugin-integration/Release/appqt_rainmeter.exe`，相邻目录包含更新后的 Runner 和插件资源。

用户默认 PowerShell 环境此前出现 PSReadLine 信任提示，未代替用户选择“始终运行/运行一次”，也未修改执行策略或证书。因此默认 PowerShell 的人工信任及更新安装后的完整复测仍待用户确认。此次不扩展到其他快捷键或清屏行为的改造。
