# 0.8.0.3 修复版

继承 docs/PLUGIN_FIXES.md 中的修复。应用、MSIX 清单及终端插件统一为 0.8.0.3。

## 已验证

- Release 编译成功，MSIX 构建及签名成功。
- 安装命令成功返回，Windows 注册版本为 0.8.0.3。
- 安装包包含两个插件的清单与 QML 页面、sidebar-terminal-runner.exe。
- 插件 QML 静态检查未报告语法或属性赋值错误。

## 桌面验收限制

已按 Computer Use 技能初始化 @oai/sky 并枚举应用和窗口。侧栏进程运行，但 list_windows/list_apps 未返回可操作的侧栏窗口；尝试通过安装路径启动定位后，工具报告 `launched app did not expose a targetable window`。

因此本次未完成实际点击、自动隐藏往返、插件页面显示及视觉协调性验收。构建与进程运行不等于 UI 验收通过。应用使用 Qt.Tool 窗口，是否与工具枚举过滤有关尚未确认。

后续需要在工具能定位窗口后验证：插件导航与重试、设置保持展开、手动收起后移出再进入、滚动无可见滚动条、窄屏与紧凑布局、终端 Tab 补全和停止快捷键。
