# 终端插件整合进度 · 2026-10-02

后续版本整合：上述代码已纳入 [0.8.1.3 本地审核版](RELEASE_0.8.1.3.md)。本文中的 0.8.1.2-preview 是终端实现阶段的测试快照；原生交互人工验收边界不因版本升级而解除。

用户本轮要求补齐旧终端未完成项。仅修改本地代码和隔离测试，未推送、未安装 MSIX、未启动主应用，也未改变设备、驱动、电源或发布者信任设置。

## 已实现

| 项目 | 结果与边界 |
| --- | --- |
| 原生传输 | ConPTY 创建、暂停子进程先入 Job、原始输入、尺寸同步、独立通信线程、关闭与管道回退；不承诺所有 TUI 兼容 |
| VT 屏幕 | 增量 UTF-8、CJK 宽字符/组合字符、颜色与样式、光标/清屏、滚动区域、有界历史、替代屏幕及查询回复 |
| 字符网格 | QQuickPaintedItem 只绘制可见区域，逐字符定位，中文与粗体不挤压后续光标；无需创建数千历史行 QML 控件 |
| 输入 | 原生 Tab/Shift+Tab、方向键/功能键/Ctrl 组合、IME 提交；Ctrl+C 是程序中断，工具栏停止回收会话 |
| 辅助补全 | 命令/PATH/本地文件/常用参数，完整预览、引号路径、历史搜索；插入草稿不隐式执行 |
| 复制粘贴 | 选择输出、复制全部、括号粘贴、多行确认；不自动读取/保存剪贴板 |
| 生命周期 | 异步启动、有界缓冲、结构化 success/failed/cancelled/timeout/reset/crashed/start-failed；兼容命令与原生会话分开，不猜测 REPL 命令边界 |
| 灵动岛 | 实时耗时、会话/命令结果、取消/超时/失败分级事件；不包含命令、环境值或输出内容 |
| 日志 | 独立轮转日志、输入仅记录字节数，兼容命令仅长度/短指纹、生命周期及错误码 |
| 构建资产 | 每次构建主应用都同步插件目录，不再只有重新链接 EXE 后才复制 QML |

## 自动验证

`SIDEBAR_BUILD_TERMINAL_TESTS=ON` 提供三个隔离目标：

- `sidebar-terminal-screen-test`：无进程/设备；UTF-8 分字节、中文/emoji、组合字符、HTML 转义、颜色、光标、字符插删、滚动区域、2000 行上限、替代缓冲、查询、OSC/DCS 过滤和尺寸边界。
- `sidebar-terminal-host-test <workspace-build-root> --pipe-only`：测试目录在工作区临时目录，用默认 PowerShell 的兼容模式执行回显/失败/睡眠；验证中文、旧完成标记误识别防护、超时、取消、重置、补全、日志不含命令/输出。临时目录随退出回收。
- `sidebar-terminal-ui-test <plugin-Main.qml> <workspace-build-root>`：完全模拟宿主，软件离屏绘制，不启动 Shell；验证固定宽字符网格背景像素、不开机自动启动、Tab/Shift+Tab 焦点、方向键/Ctrl+C、IME 提交、括号粘贴、补全预览、草稿不附加回车、兼容模式、不同宽度和多行确认框；要求零 QML warnings。截图输出 `terminal-native-preview.png`。

最终结果：主应用及 Runner Release 构建退出码 0；以上三个终端目标均退出码 0，UI 零 QML warnings，含清屏保留终端模式、复制更新、外部命令退出码 7、灵动岛分级事件及不泄漏命令内容的断言。原有 `sidebar-island-core-test` 与 `sidebar-stability-test` 回归均退出码 0，未调用真实系统接口。Git 空白检查在允许 Windows CRLF 的配置下通过；构建目录插件 QML 与源文件 SHA-256 一致。主应用仍保留 0.8.1.2-preview 身份，未制作新正式发行包。

## 人工检查处停止

默认 Windows Terminal Profile 指向 Scoop 的 PowerShell 7。真实 ConPTY 启动测试触发 PSReadLine 的 Microsoft 发布者信任提示，原来的“等待 Shell 注入标记”握手因此超时。已改成首个终端输出建立传输，让该提示可见且可由用户操作；不自动输入 Run once / Always run，也不修改执行策略或证书信任。

默认 shell 的完整原生实测因此没有通过；按用户“需要人工检查就停下”要求，停止此项真实交互测试，不持续重启或试图自动绕过提示。无 `--pipe-only` 的 host 测试会尝试原生模式，必须在人工处理本机问题后再运行。

下一次人工验收需确认：

1. 按用户自己的信任选择处理 PSReadLine 提示，确认原生会话正常进入提示符。
2. 实际中文输入法候选窗口、连续输入、Tab 补全、Ctrl+C、长时间交互及窗口缩放。
3. CMD / Windows PowerShell / PowerShell 7 和真实 REPL/TUI 的交互、正常退出以及关闭时无残留进程。
4. 应用内卡片实际尺寸、不同 DPI、多屏下光标/IME 坐标与鼠标命中。

## 不应混淆的限制

此插件仍以当前用户权限运行，不是安全沙盒，不能交给不受信任代码。ConPTY 是终端传输而不是权限隔离；原生 cwd/env 由 Shell 持有，辅助补全仅知道启动目录。完整字素、历史列宽重排、鼠标报告和所有 xterm 扩展未完成，不宣称 100% 原生 Windows Terminal / xterm 兼容。
