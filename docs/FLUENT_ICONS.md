# Microsoft Fluent System Icons

采用微软官方 `@fluentui/svg-icons` 1.1.343，MIT 许可证。

来源：https://github.com/microsoft/fluentui-system-icons

完整发布包及 20,762 个 SVG 位于仓库外工作区 `work/vendor/fluent-svg-icons-1.1.343/package/icons`。下载时按 npm 发布记录校验 SHA-512，原始压缩包仍保留。包含发布包提供的所有尺寸与样式；不是 WinUI 原生控件全集。

首批 16 个 20 px Regular 图标位于 `assets/fluent`，与官方 LICENSE 一同打包。只将实际选用的 SVG 编入应用，完整资源缓存不提交 Git、不带进安装包。之后可从全集复制更多图标进入此目录，CMake 会自动收集。

`FluentIcon.qml` 统一尺寸和主题着色；C++ `FluentIconProvider` 只读取内嵌资源，用 SVG 原始形状和 alpha 蒙版着色，不改写原文件。调用方式：`FluentIcon { name: "timer"; tint: themeManager.accent }`。目前接入内置导航、设置与专注计时标题；插件自定义符号保留兼容。

重新下载其他版本：`powershell.exe -NoProfile -File scripts/download-fluent-icons.ps1 -Version <version>`。脚本不覆盖已有缓存/图标，更新需先人工审阅。发布包没有附带 LICENSE 文件，因此从微软仓库对应版本标签获取许可证。

验证：Release 构建、SVG 资源渲染探针和全部页面冒烟测试通过。修复 Image 隐式尺寸只读导致的初次组件加载失败；图标由 Item 管理尺寸。当前未完成图标版实际桌面截图检查，也未推送远程或覆盖已安装 MSIX。
