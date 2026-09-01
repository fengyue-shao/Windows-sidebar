# 受限终端插件

## 定位

受限终端是 Windows Sidebar 插件系统的首个后端型插件。它提供嵌入侧栏的持续 Shell 会话、智能补全和独立调试日志，同时通过单独的 Runner 进程限制命令对宿主应用的影响。

当前隔离基于 Windows Job Object、PowerShell Constrained Language、受控环境继承和应用专用工作目录。它用于降低误操作和失控进程风险，但不是 AppContainer、Windows Sandbox 或虚拟机级别的安全边界。

## 组成

- `plugins/com.windows-sidebar.terminal/ui/Main.qml`：内联终端界面、命令面板、历史导航和状态提示。
- `include/terminalmanager.h`、`src/terminalmanager.cpp`：会话生命周期、输出解析、智能补全、目录和环境同步。
- `runner/terminalrunner.cpp`：独立 Shell Runner、Job Object、编码转换和宿主通信协议。
- `sidebar-terminal-runner.exe`：构建后与主程序放在同一运行目录。

宿主与 Runner 使用标准输入输出通信。用户命令和内部控制消息使用不同协议：用户命令产生完成标记与退出状态；目录和环境变量同步不进入用户历史，也不污染终端输出。

## 会话行为

Runner 在首次执行命令时启动并保持同一个 PowerShell 或 CMD 进程。以下状态可跨命令保留：

- PowerShell 函数和普通变量；
- 会话环境变量；
- 当前工作目录；
- Shell 内部状态。

`cd` 和 `Set-Location` 由宿主验证，目标必须是沙盒根目录或其已有子目录。环境变量支持 PowerShell 的 `$env:NAME = "value"` 与 CMD 的 `set NAME=value` 形式。应用重启后，会话状态不会恢复。

## 输入与补全

- `Enter`：执行命令。
- `↑` / `↓`：浏览历史或命令面板候选项。
- `Tab`：接受当前补全项。
- `Ctrl+R`：反向搜索本次会话历史。
- `Ctrl+C` / `Esc`：终止当前命令及 Runner 作业树。
- `Ctrl+L`：清空显示，不清除历史。

补全数据来自：

- 内置斜杠命令；
- PowerShell/CMD 常用命令；
- 系统 `PATH` 中的 `.exe`、`.cmd`、`.bat` 和 `.com`；
- 当前沙盒目录中的文件与文件夹。

候选项在终端内部展开，并标注“命令”“文件”或“目录”及其来源。

### 内置命令

- `/help`：显示快捷键。
- `/status`：显示 Shell、传输模式、编码、环境变量数量和当前目录。
- `/clear`：清空终端显示。
- `/history`：显示本次会话命令历史。
- `/logs`：打开插件日志。
- `/stop`：终止当前命令。
- `/reset`：清空显示和输入历史。

## Shell 与编码

插件读取 Windows Terminal 的 `settings.json`，尝试继承默认 Profile。当前支持 PowerShell 7、Windows PowerShell 和 CMD；无法识别时回退到 Windows PowerShell。

Runner 使用 UTF-8 与宿主通信。PowerShell 输出设为 PlainText，宿主增量解码 UTF-8，并过滤 CSI/OSC 等终端控制序列。CMD 使用其 Unicode 输出模式转换为 UTF-8。

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
- 单条命令默认最多运行 60 秒；
- Runner 关闭时回收作业树；
- 宿主最多保留约 256 KiB 输出，超过后截断较早内容。

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

- 当前仍使用持续管道传输，不是完整的伪控制台。
- ANSI 序列会被过滤，而不是由 VT 屏幕模型解释，因此颜色、光标定位和全屏界面不可用。
- `vim`、`ssh`、交互式 Python、Codex CLI 和 Claude Code 等程序尚不能获得原生 TTY 体验。
- GUI 应用仍属于 Runner 作业树；退出侧栏或重建 Runner 时，相关子进程可能一起退出。
- PowerShell Constrained Language 并不能替代操作系统级文件系统隔离。

## ConPTY 路线

系统启动时会动态检查 `CreatePseudoConsole`、`ResizePseudoConsole` 和 `ClosePseudoConsole`。后续实现顺序：

1. 在 Runner 中创建 ConPTY 与同步输入输出管道。
2. 使用 `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE` 启动默认 Shell。
3. 将原始键盘输入、Ctrl 组合键和窗口字符尺寸发送给 ConPTY。
4. 实现最小 VT 屏幕模型，处理颜色、光标、清屏和替代屏幕缓冲区。
5. 增加普通管道回退，并分别测试 PowerShell、CMD、REPL 与全屏 TUI。

关闭伪控制台时必须先协调输出读取管道，避免旧版本 Windows 上发生关闭死锁。
