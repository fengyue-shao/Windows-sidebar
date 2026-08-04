# 扩展与维护指南

## 添加新的周期监控模块

如果数据只需要一个或少量 Q_PROPERTY，可继承 `MonitorBase`：

1. 在 `include/` 创建头文件，声明属性和变化信号；
2. 在 `src/` 实现 `updateData()`；
3. 只有数值实际变化时才更新成员并发出信号；
4. 在 `src/main.cpp` 创建对象；
5. 调用 `monitorScheduler.addModule(&module, intervalMs)`；
6. 使用 `setContextProperty` 暴露给 QML；
7. 将文件加入 `CMakeLists.txt` 的 `SOURCES`。

如果需要向 QML 提供多行数据，应参考 `DiskMonitor`，继承 `QAbstractListModel` 与 `PeriodicModule`，并正确实现 model reset、insert/remove 或 `dataChanged`。

不要在新增监控器中再创建独立 QTimer；周期刷新统一交给 `MonitorScheduler`。

## 添加新的页面分区

1. 在 `qml/` 创建独立组件；
2. 将组件加入 `CMakeLists.txt` 的 `QML_FILES`；
3. 在 `Sidebar.qml` 的 `StackLayout` 中加入页面；
4. 在 `SystemControlBar.qml` 中加入相同索引的按钮；
5. 保证 `currentSection/currentIndex` 的索引约定一致。

媒体控件目前与通知位于同一页面，但仍是独立 `MediaBar` 组件。后续新增功能时应保持功能组件隔离，避免复用通知卡片内部状态或未读样式。

## 添加主题颜色

1. 在 `ThemeManager` 添加 `Q_PROPERTY(QColor ...)`；
2. 为属性实现 getter；
3. 在 `choose(light, dark)` 中定义日间与夜间颜色；
4. QML 通过 `themeManager.<name>` 使用颜色；
5. 避免直接在 QML 中写固定十六进制颜色。

所有主题属性使用同一个 `themeChanged` 信号，因此切换模式后 QML 绑定会统一更新。

## Windows 异步接口注意事项

- WinRT 入口前必须已经初始化 apartment；
- 异步回调可能不在 Qt GUI 线程；
- 更新 QObject/QML 状态时使用 queued connection 或 `QMetaObject::invokeMethod`；
- 长生命周期异步操作应使用 `QPointer` 检查目标是否仍存在；
- 注册 WinRT event token 后，应在析构或切换会话时注销；
- 需要包身份的能力必须同时更新 `AppxManifest.xml`。

## 通知模型约定

- Windows 通知 ID 是去重和同步删除的关键字段；
- 系统更新同一 ID 时应更新现有行，不应重复插入；
- 用户删除时先更新本地模型，再通过 `removalRequested` 请求系统删除；
- 系统删除事件只调用 `removeById`，避免再次发出删除请求形成回路；
- 新增模型角色后，需要同步修改枚举、`data()` 和 `roleNames()`。

## 媒体模块约定

- WinRT 媒体会话操作保留在 `MediaWorker`；
- QML 只调用 `MediaMonitor` 的公开槽；
- 封面更新后递增 `coverVersion`，确保 QML 图片缓存失效；
- 标题、艺术家、专辑和提供者可能很长，应继续使用 `MediaMarquee`；
- 浏览器媒体封面可能是矩形，界面不要假设封面恒为正方形。

## UI 布局注意事项

- 速率、标题等动态文本不能决定固定分隔线的位置；应使用中心锚点或明确列宽；
- 系统监控页可以滚动，磁盘分区数量增加时不应挤压底部控制栏；
- 媒体区域无活动会话时通过 `visible` 和高度绑定完全收起；
- 底部控制栏属于全局导航和设置，不应承载业务模块内部状态；
- 新组件颜色、边框、文本层级应统一绑定 ThemeManager。

## 修改后的验证清单

### 编译层

- CMake 配置成功；
- `appqt_rainmeter` 目标编译成功；
- QML cache generation 无错误；
- `package_msix` 创建并签名成功。

### 功能层

- 展开/折叠后窗口仍贴合屏幕右侧；
- CPU、内存、GPU、网络、磁盘持续更新；
- 下载速率变化时中间分隔线不移动；
- 媒体信息更新，播放控制可用；
- 长媒体文本自动滚动；
- 新通知出现，重启后可加载当前系统通知；
- 单条删除和清空能够同步到 Windows；
- 三种主题下文字与背景均可读；
- 设置菜单能够退出进程。

### 分发层

- 提高 MSIX 版本；
- 在干净构建目录运行一键脚本；
- 在测试机信任开发证书；
- 从开始菜单启动安装后的应用。

