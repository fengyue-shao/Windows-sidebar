# Windows Sidebar 插件系统设计

## 1. 目标与边界

插件系统用于扩展侧栏页面、概览卡片、快捷操作和后台能力，同时避免插件崩溃、阻塞或越权影响主程序。

第一阶段不支持插件直接加载任意 C++ DLL 到主进程。第三方后端默认运行在独立进程，通过受控 RPC 与主程序通信；主进程只负责发现插件、权限判断、界面托管和生命周期管理。

首个验证插件建议为“受限终端”：它能覆盖流式输出、长任务、中止、文件权限、用户确认和进程隔离等核心场景。

## 2. 插件类型

一个插件可以声明一种或多种贡献：

- `page`：出现在底部导航中的完整页面。
- `card`：插入概览页的卡片。
- `quickAction`：控制中心中的快捷按钮。
- `service`：无界面的后台服务。
- `settings`：插件自己的设置页面。

内置功能也逐步迁移到相同的贡献模型，但仍可编译进主程序。这样插件系统不是一套只供第三方使用的旁路架构。

## 3. 目录与清单

插件安装目录：

```text
plugins/
  com.example.terminal/
    plugin.json
    ui/
      Main.qml
      Settings.qml
    backend/
      terminal-runner.exe
    assets/
      icon.svg
    signatures/
      manifest.sig
```

建议的 `plugin.json`：

```json
{
  "schemaVersion": 1,
  "id": "com.example.terminal",
  "name": "受限终端",
  "version": "0.1.0",
  "minimumHostVersion": "0.9.0",
  "publisher": "Example",
  "entry": {
    "ui": "ui/Main.qml",
    "backend": "backend/terminal-runner.exe"
  },
  "contributes": {
    "pages": [
      { "id": "terminal", "title": "终端", "icon": "assets/icon.svg", "order": 500 }
    ],
    "settings": "ui/Settings.qml"
  },
  "permissions": [
    "process.execute",
    "filesystem.workspace.read",
    "filesystem.workspace.write"
  ]
}
```

约束：插件 ID 永久稳定；路径必须是插件目录内的相对路径；未知字段可以忽略，未知权限必须拒绝；主版本不兼容时禁用插件，而不是尝试加载。

## 4. 主程序架构

```text
QML Sidebar / Dashboard / Settings
                │
         PluginRegistryModel
                │
   ┌────────────┼─────────────┐
PluginDiscovery PermissionStore PluginSupervisor
                                      │
                                Local RPC channel
                                      │
                                Plugin Runner
```

### PluginDiscovery

- 扫描内置插件目录和用户插件目录。
- 解析并校验清单、规范化路径、防止 `..` 越界。
- 检查宿主版本、清单版本、文件哈希和签名状态。
- 只产出不可变的 `PluginDescriptor`，不执行插件代码。

### PluginRegistryModel

- 向 QML 暴露动态页面、卡片和快捷操作模型。
- 保存启用状态、排序、固定位置和故障状态。
- 使用字符串页面 ID，不再依赖 `currentSection` 的固定整数含义。
- 内置页面注册为 `builtin.dashboard`、`builtin.monitor` 等。

### PermissionStore

- 保存“插件 ID + 权限 + 作用域”的授权结果。
- 区分 `allowOnce`、`allowAlways` 和 `deny`。
- 文件权限必须携带规范化目录范围；不能只授予抽象的全盘读写。
- 权限提升必须由宿主显示确认界面，插件不能伪造授权 UI。

### PluginSupervisor

- 启动、监控和停止插件后端进程。
- 每个插件单独进程、单独 Job Object、单独命名管道。
- 限制子进程、内存、CPU 时间和退出时清理。
- 心跳超时、崩溃次数过多时自动禁用，并在设置页显示原因。

## 5. UI 托管方案

不允许第三方 QML 直接获得 `themeManager`、`systemTrayManager` 等完整根上下文，否则插件可以绕过权限层调用系统功能。

推荐两层方案：

1. 内置/受信任插件可以通过受控 `QQmlComponent` 加载，使用专用 `QQmlContext`。
2. 第三方插件优先使用宿主提供的声明式 UI schema；确需 QML 时使用独立 `QQmlEngine`、受限 import path 和最小上下文对象。

插件 UI 只能拿到：

- `plugin`：插件元数据和生命周期状态。
- `bridge`：带权限检查的异步 RPC 门面。
- `tokens`：颜色、圆角、间距和字体令牌，只读。
- `navigation`：请求宿主导航，不可直接操作主窗口。

禁止插件 import 本地任意路径、创建顶层窗口或直接访问主程序 QObject。

## 6. RPC 协议

本地通信使用命名管道上的 JSON-RPC 2.0。每次启动由宿主生成随机管道名和一次性令牌，通过继承受限句柄或启动参数交给插件。

基础方法：

- `plugin.initialize`：协商协议版本和能力。
- `plugin.shutdown`：请求优雅退出。
- `command.invoke`：调用插件命令。
- `command.cancel`：中止长任务。
- `event.subscribe`：订阅允许的宿主事件。
- `permission.request`：请求新增权限，由宿主决定。
- `stream.open/data/close`：传输终端输出等流式数据。

每个请求包含递增 ID、超时和最大消息尺寸。宿主对方法名、参数结构、速率和输出总量进行限制。

## 7. 权限模型

首批权限建议：

```text
clipboard.read / clipboard.write
notifications.read
network.status / network.access
filesystem.pluginData.read / write
filesystem.workspace.read / write
process.execute
process.child
shell.powershell
system.audio.control
system.radio.control
```

原则：

- 插件私有数据目录默认可读写，无需反复确认。
- 工作区访问必须由用户选择目录。
- `process.execute` 不等于可以执行任意路径；需要可执行文件范围。
- 网络和文件权限分开授权。
- 管理员提权不作为 v1 能力。
- 所有敏感调用必须在宿主侧再次检查，不能只依赖插件清单。

## 8. 受限终端作为首个插件

终端插件由 QML 前端和独立 Runner 组成：

- 前端：命令输入、输出流、停止按钮、工作目录、历史记录。
- Runner：创建 PowerShell 子进程并放入 Job Object。
- 默认工作目录为用户明确选择的目录。
- 禁止继承主程序敏感句柄和环境变量。
- 设置执行超时、输出上限、进程树关闭策略。
- 写文件、网络访问、安装软件等行为在能力层请求授权。

Job Object 和命令过滤只能构成“受限执行环境”，不能被宣传为强安全沙盒。强隔离版本应进一步使用 AppContainer 或 Windows Sandbox；这可以作为 v2，而不是阻塞插件系统 v1。

## 9. 对现有代码的改造点

当前代码需要以下结构性调整：

1. 将 `main.cpp` 中逐个创建并注入根上下文的对象拆分为 `HostServices`。
2. 新增 `PluginManager`，组合 Discovery、Registry、PermissionStore 和 Supervisor。
3. 将 `UiSettings::currentSection` 从固定 `int` 迁移为稳定字符串 ID，同时兼容旧设置迁移。
4. 将 `Sidebar.qml` 固定的 `StackLayout` 攁为基于注册模型的 `Loader`/`Repeater`。
5. 将 `SystemControlBar.qml` 固定按钮改为导航模型，并设置最大可见数量和“更多”入口。
6. 将主题参数整理为只读 `UiTokens`，供插件安全使用。
7. 在设置页增加插件管理：启停、权限、版本、故障、卸载和日志。

不建议一次性把所有内置页面迁移。先让动态页与现有固定页共存，验证稳定后再逐步统一。

## 10. 分阶段实施

### 阶段 A：注册表与动态导航

- 定义清单 schema、Descriptor 和 RegistryModel。
- 加载一个无后端的内置示例插件。
- 支持动态页面、启停、排序和设置持久化。
- 保留原有 5 个页面，降低改造风险。

验收：新增页面无需修改 `Sidebar.qml` 和 `SystemControlBar.qml` 的按钮列表。

当前进度：已完成 UI 插件清单扫描、路径校验、动态页面与导航、设置页启停和 QSettings 持久化。当前 Loader 仍运行在宿主 QML 引擎中，因此仅允许随项目提供的受信任 UI 插件。

### 阶段 B：独立进程与 RPC

- 实现命名管道 JSON-RPC。
- 实现 Supervisor、心跳、超时、崩溃恢复和日志。
- 制作 echo 示例后端，验证请求、事件和流。

验收：插件崩溃不会使侧栏退出，禁用插件后进程和句柄全部释放。

### 阶段 C：权限系统

- 实现 PermissionStore 和宿主确认界面。
- 支持插件私有目录、用户选择工作区和一次性授权。
- 加入审计日志和权限撤销。

验收：未授权的 RPC 即使由恶意插件直接构造也会被宿主拒绝。

### 阶段 D：受限终端插件

- 实现 Runner、流式输出、取消和 Job Object。
- 支持 PowerShell，会话历史和工作目录。
- 对危险能力做权限提示并设置资源上限。

验收：可运行普通命令；中止会杀掉完整子进程树；无法访问授权目录外的宿主代理文件 API。

### 阶段 E：打包与生态

- 定义 `.sidebar-plugin` 安装包。
- 校验哈希、签名、更新来源和版本回滚。
- 提供插件模板、SDK 文档和兼容性测试工具。

## 11. 第一轮建议任务拆分

1. `PluginDescriptor` 与严格 JSON 校验。
2. `PluginDiscovery` 和用户插件目录。
3. `PluginRegistryModel` 与 QSettings 持久化。
4. 动态导航适配层，先插入一个“示例插件”页面。
5. 插件管理设置页。
6. 命名管道 RPC 原型。
7. echo Runner。
8. 权限确认原型。
9. 受限终端 MVP。

阶段 A 应作为下一次开发的明确起点。它不引入外部进程和安全风险，但会验证最关键的 UI 与注册架构是否成立。
