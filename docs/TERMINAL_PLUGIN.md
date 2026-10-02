# 受限终端插件

## 定位

受限终端是 Windows Sidebar 插件系统的首个后端型插件。它提供嵌入侧栏的持续 Shell 会话、智能补全和独立调试日志，同时通过单独的 Runner 进程限制命令对宿主应用的影响。

当前隔离基于 Windows Job Object、PowerShell Constrained Language、受控环境继承和应用专用工作目录。它用于降低误操作和失控进程风险，但不是 AppContainer、Windows Sandbox 或虚拟机级别的安全边界。

## 组成

- `plugins/com.windows-sidebar.terminal/ui/Main.qml`：内联终端界面、命令面板、历史导航和状态提示。
- `include/terminalmanager.h`、`src/terminalmanager.cpp`：会话生命周期、输出解析、智能补全、目录和环境同步。
- `runner/terminalrunner.cpp`：独立 Shell Runner、Job Object、编码转换和宿主通信协议。
- `include/terminalscreen.h`、`src/terminalscreen.cpp`：有界的增量 UTF-8 / VT 字符屏幕模型。
- `include/terminalcanvas.h`、`src/terminalcanvas.cpp`：仅绘制可见字符网格，避免中文、粗体和字体回退引起光标偏移。
- `sidebar-terminal-runner.exe`：构建后与主程序放在同一运行目录。

宿主与 Runner 使用标准输入输出通信，启动首行协商传输模式。原生 ConPTY 传递原始输入和 VT 输出；兼容管道使用每条命令独立的随机完成标记，目录和环境同步走独立控制消息。标记用于减少误识别，不是防恶意 Shell 的安全边界。

## 会话行为

加载插件不会自动启动 Shell；原生模式由“启动会话”手动打开，兼容模式也可以首次执行时启动。两种模式都保持同一个 PowerShell 或 CMD 进程。以下状态可跨命令保留：

- PowerShell 函数和普通变量；
- 会话环境变量；
- 当前工作目录；
- Shell 内部状态。

兼容模式的独立 `cd` / `Set-Location` 由宿主验证，目标必须是默认工作目录或其已有子目录；独立环境赋值同步到 Shell。原生模式所有输入交给 Shell，工作目录、环境、函数及历史由 Shell 自己管理；顶部显示的是“启动目录”，不能冒充实时 cwd。应用重启后，会话状态不会恢复。

## 输入与补全

原生模式：Enter、Tab、方向键、功能键、Ctrl 组合键直接交给前台程序；Tab 使用 Shell 本身的补全，Ctrl+C 发送中断，不强制销毁会话。中文输入使用 TextInput 的 IME 提交事件，候选期不抢占按键。

`Ctrl+Space` 打开辅助输入；辅助框 Tab 接受预览，Shift+Tab 切换候选，Ctrl+R 搜索本次辅助历史。辅助框 Enter /“插入草稿”仅插入文字，不自动附加回车；原生历史与辅助历史分开。

兼容模式：Enter 执行、Tab 接受预览、上下键选择候选/历史、Ctrl+R 搜索历史、Ctrl+C / Esc 结束任务、Ctrl+L 清屏。

原生 Ctrl+Shift+C 复制全部输出，Ctrl+V 粘贴，Ctrl+Shift+L 清空本地显示；“选择输出”支持选择复制。剪贴板包含换行时先询问用户，前台程序声明 bracketed-paste 时加相应标记；这不是防止命令执行的安全边界。“结束会话”先请求 Runner 关闭，800 ms 未退出则回收作业树。

补全数据来自：

- 内置斜杠命令；
- PowerShell/CMD 常用命令；
- 系统 `PATH` 中的 `.exe`、`.cmd`、`.bat` 和 `.com`；
- 当前沙盒目录中的文件与文件夹。
- 常用 PowerShell 参数的静态目录（如 `Get-ChildItem -Recurse`），不执行用户输入或补全脚本。

候选项在辅助输入区展开，带完整命令预览、类型与来源。路径处理引号、空格及 PowerShell 转义，最多 8 个结果，UI 展示 6 个；历史限 100 条，仅存内存。原生 Shell 可能切换目录，辅助文件补全仍以宿主的启动目录为基准，不能替代 Shell 的实时补全。

### 内置命令

- `/help`：显示快捷键。
- `/status`：显示 Shell、传输模式、编码、环境变量数量和当前目录。
- `/clear`：清空终端显示。
- `/history`：显示本次会话命令历史。
- `/logs`：打开插件日志。
- `/stop`：终止当前命令。
- `/reset`：终止 Shell，清空会话环境变量，恢复初始工作目录，并清空显示和输入历史。

## Shell 与编码

插件读取 Windows Terminal 的 `settings.json`，尝试继承默认 Profile。当前支持 PowerShell 7、Windows PowerShell 和 CMD；无法识别时回退到 Windows PowerShell。

Runner 使用 UTF-8 与宿主通信。ConPTY 的 UTF-8 / VT 输出交给字符网格；兼容模式 PowerShell 使用 PlainText、CMD 使用 Unicode 输出再转 UTF-8。原生支持 16 / 256 / truecolor、粗体/斜体/下划线、光标移动、擦除、插入/删除字符与行、滚动区域、替代缓冲区、设备状态查询和标题。OSC 链接、剪贴板写入及 DCS 等不执行外部动作。

Shell 仍使用 `-NoProfile` 与进程级 Restricted 执行策略，不修改用户或机器策略。原生 PowerShell 自动加载 PSReadLine 时若出现发布者信任提示，必须由用户自行处理；应用显示提示，不自动选择、添加证书或绕过策略。Shell 初始化成功后设置 ConstrainedLanguage，不承诺启动提示阶段或任意子进程构成安全隔离。

## 环境继承

Runner 不复制整个宿主环境，而是继承 Windows 命令和桌面应用常用的必要变量，包括：

- `SystemRoot`、`WINDIR`、`SystemDrive` 和 `ComSpec`；
- `PATH`、`PATHEXT` 和 `PSModulePath`；
- `USERPROFILE`、`APPDATA`、`LOCALAPPDATA`、`HOMEDRIVE` 和 `HOMEPATH`；
- Program Files、用户、域和处理器相关变量。

`USERPROFILE`、`APPDATA` 和 `LOCALAPPDATA` 对 VS Code/Electron 等桌面应用正常启动是必需的。

## 资源限制

当前采用“桌面应用兼容档”：

- 最多 32 个活动进程；
- 单进程内存最多 1 GB；
- Runner 作业树总内存最多 4 GB；
- 兼容模式单条命令默认最多运行 60 秒，可选择 30 秒 / 60 秒 / 5 分钟 / 关闭；原生交互会话不设空闲或“每条命令”超时；
- Runner 关闭时回收作业树；
- 纯文本显示保留约 256K 字符；网格保留最多 2000 行历史，尺寸上限 240×100，只绘制可见区域；
- 启动握手有超时，输入与协议缓冲有上限。

这些限制允许 VS Code 一类多进程桌面应用启动，同时仍为失控命令提供基本上限。

## 日志与排查

日志默认位于：

```text
%LOCALAPPDATA%\WindowsSidebar\WindowsSidebar\logs\terminal-plugin.log
```

日志超过 2 MB 后轮转到 `.1` 文件。记录内容包括：

- 宿主和 Runner 启动、退出与错误；
- Shell 类型、工作目录和 ConPTY 可用性；
- 命令长度与 SHA-256 短指纹，不记录完整命令；
- 退出码、耗时和输出字节数；
- Job Object 限制应用结果。

排查顺序：

1. 用 `/status` 检查 Shell、当前目录和传输模式。
2. 用 `/logs` 打开最新日志。
3. 检查 `runner.start` 后是否出现 `job.limit.applied` 或 `job.*.failed`。
4. GUI 应用启动失败时，检查 `USERPROFILE`、`APPDATA`、进程数量与内存限制。
5. 输出乱码时，分别测试中文字符串、`Get-ChildItem` 和目标程序的 `--version`。

## 已知限制

- 已实现 ConPTY 与有界 VT 模型，但不是完整 xterm：复杂 emoji / ZWJ 字素、全部字符集/鼠标模式及历史缩放重排尚不完整；改变列数不进行全文重排。
- 原生无法可靠判断任意 REPL/TUI 中“一条命令何时结束”；只报告会话结果，不伪造命令完成事件。兼容模式报告结构化命令结果。
- 第三方 `vim` / `ssh` / Python REPL / Codex CLI / Claude Code 的完整交互兼容性尚未全部实测；不能把传输实现等同于所有程序通过验收。
- GUI 应用仍属于 Runner 作业树；退出侧栏或重建 Runner 时，相关子进程可能一起退出。
- PowerShell Constrained Language 并不能替代操作系统级文件系统隔离。
- 单独 `cd` 和补全的路径检查不限制其他 Shell 命令或子进程的文件访问；运行程序仍拥有当前用户权限。

## ConPTY 实现与验收

Runner 动态解析 `CreatePseudoConsole`、`ResizePseudoConsole` 和 `ClosePseudoConsole`。实现包括：

1. 同步管道，独立输入控制和输出读取线程；不分离悬空读取线程。
2. `STARTUPINFOEX / PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE` 创建暂停的子进程，加入 Job 后再恢复。
3. 原始输入与字符尺寸同步，增量 UTF-8 网格、CJK 宽字符与可见区域绘制。
4. 自动模式启动失败时回退管道；明确选 ConPTY 则报告失败，不静默变换模式。
5. 首个输出触发传输协商，立即显示初始化 / 信任提示，不以注入命令完成为前提阻塞输入。

关闭伪控制台时必须先协调输出读取管道，避免旧版本 Windows 上发生关闭死锁。

官方依据：[创建伪控制台会话](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session)、[Console VT sequences](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences)。当前自动测试及人工验收边界见 [本次进度](TERMINAL_PROGRESS_2026-10-02.md)。
