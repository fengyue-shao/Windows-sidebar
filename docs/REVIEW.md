# 主应用最小仓库审核交付

审核标识：`host-minimal-20261016`。用户确认的范围是“只精简主体仓库结构，保留现有主应用功能”。前一阶段只交付审核快照；2026-10-16 用户进一步授权将该主体版本合并到 `main`、进行静态检查，通过后推送 `origin/main`。本次仍不重新安装、不变更软件版本。

## 审核顺序

1. 看 [README](../README.md) 确认仓库定位和目录职责。
2. 看 [功能与版本矩阵](FEATURES.md)，区分安装版、源码开发增量和可选预览。
3. 看 [架构与 JSON 接口](ARCHITECTURE.md)，确认主应用不承载具体插件实现。
4. 看 [构建与测试](BUILD.md)，复现独立主体构建；本页记录本轮实际证据及恢复位置。

## 软件版本

| 对象 | 版本 / 状态 |
| --- | --- |
| 主体 CMake、MSIX 清单 | `0.8.2.0`，本轮没有自行升版 |
| 运行时 | `0.8.2.0-preview`，当前源码审核候选，不是新正式发行 |
| EXE 版本资源 | 已读取最终二进制核验：文件版本 `0.8.2.0`、产品版本 `0.8.2.0-preview`、产品名 Windows Sidebar、预发布标志 true；由 CMake 版本生成 |
| 本机安装版 | 2026-10-16 只读核验为 WindowsSidebar `0.8.2.0`，本轮未替换 |
| 独立终端 | `0.8.3.1` 预览，源码与历史测试归档在主体仓库之外，不计入主体版本 |
| 审核快照 Git 基线 | 分支 `codex-do`，HEAD `8f3fc45`，当时含未提交修改；此行为整理前基线，不代表随后合并后的 HEAD |

同号安装版不包含所有当前未发布增量。源码中的自由桌面卡片、通用插件多版本选择及结构化结果协议等应按 FEATURES 中的“开发/预览”边界审核。确定发行后，应另选新版本并重新打包，而不是把本审核候选冒充同号安装版。

## 保留内容

| 目录 | 审核用途 |
| --- | --- |
| `src/`、`include/` | 主体系统服务、状态/布局模型、窗口控制、通用插件协议 |
| `qml/` | 当前可达界面、内置卡片及仍有测试引用的画布 |
| `assets/fluent/` | 实际使用的 25 个 SVG 和 Microsoft MIT 许可证 |
| `packaging/` | MSIX 身份/能力、应用图标、Qt 配置及版本资源模板 |
| `tests/` | 主体确定性回归、mock/画布夹具、手动性能和显式集成助手 |
| `scripts/` | 安全构建入口、运行库部署、显式打包/安装、可选卡片性能测试 |
| `docs/` | 当前四篇审核文档；历史日期进度不再混入当前说明 |

主应用保留顶部栏、监控、音频/麦克风/媒体、网络/无线、通知、剪贴板/OCR、专注、唤醒、USB、灵动岛和工具箱。自由桌面卡片仍由 `--native-card-preview` 开启，未改变默认界面或扩大到所有页面。

153 个现存功能文件（全部原有 C++ 源码/头文件、现用 QML、Fluent 资源和原有打包清单/图标）已与整理前备份逐字节 SHA-256 核验一致。变更集中在构建组织、当前文档、脚本安全入口、归档以及新增 EXE 版本资源，不混入业务重写。

## 归档及恢复

整理前已备份 **295 个源码文件**，逐个核验 SHA-256；Git 目录留在原处。归档移出了 **102 个源码/文档文件**，另保存了 **331 个旧构建文件，约 71.9 MiB**。本轮没有不可恢复地删除材料，也没有改外部构建目录、已安装包、插件数据或用户设置。

归档范围：终端插件目录及其专属测试/脚本；旧日期进度文档和设计稿；默认会安装的旧一键脚本；未被引用的 `DashboardPage/SystemMonitorPanel/WidgetCard` 和四个旧媒体 SVG；仓库内被忽略的旧 `build`。当前通用插件管理、协议与工具箱测试全部保留。

恢复根目录（位于源码仓库外）：

```text
C:\Users\34575\Desktop\qt-rainmeter-local\work\backups\host-minimal-20261016\
  source/                       整理前完整源码，包括原有未提交文件
  source-manifest.json          原始逐文件大小和 SHA-256
  verification.json             原始备份逐项核验结果
  archived/                     从主体目录移出的文件
  archived/ignored-build/       原仓库内旧构建产物
  archive-plan.json             精确归档文件清单
  ignored-build-manifest.json   旧构建逐文件哈希
  git-status-before.txt         整理前 Git 状态
  working-tree-before.patch    整理前工作树补丁
  staged-before.patch          整理前暂存区补丁
```

需要找回某项时，先从 `source/` 或 `archived/` 复制到新的临时目录比对，仅恢复选定路径。不要把整个备份覆盖回当前仓库，也不要用 `git reset --hard` 丢弃此前工作。终端源码归档仍有历史的宿主协议/测试路径依赖；本轮不把它宣称为已经迁出的独立可构建插件仓库，完整依赖可从 `source/` 恢复。

## 本轮验证

**最终状态：VS2022 x64 / Release 全新构建通过，26 / 26 项主体回归通过，主应用 Qt 运行库部署校验通过。** 该构建完全不需要终端源码或 Runner；未运行显式硬件集成助手。EXE 版本属性与上表一致，已安装版仍未变。

本机验证环境：Qt 6.9.3 MSVC x64、MSVC 19.44、CMake 4.1.2、Windows SDK 10.0.26100.0。配置中的可选 Vulkan 头文件提示未阻断构建；本轮没有为此额外下载依赖。

```powershell
.\scripts\build.ps1 -QtBin 'D:\Qt\6.9.3\msvc2022_64\bin' -CMake 'D:\cmake\bin\cmake.exe' -BuildDir '..\build-host-review-vs-20261016' -Test -Deploy -Parallel 2
```

审核候选 EXE：`C:\Users\34575\Desktop\qt-rainmeter-local\build-host-review-vs-20261016\Release\appqt_rainmeter.exe`。已部署 Qt 依赖供用户手动预览，本轮没有自动启动主体，也没有将该目录当作源码仓库或安装包。

测试详情在上述构建目录的 `Testing/Temporary/LastTest.log`；另外导出 JUnit、二进制版本/哈希、源码清单及 ZIP 校验到 `work/reviews/host-minimal-20261016/`。26 项覆盖卡片/桌面布局、离屏画布、插件包/版本/进程/协议/工具箱、剪贴板结果、通知模型/监听/筛选/界面、无线状态、全屏/热键策略、蓝牙生命周期、OCR/热点异步、USB结果、音频输出与灵动岛等 mock/确定性逻辑。每项具体输出保留在测试证据中。

已完成的静态核验：主程序构建不引用 `plugins/`；所有现存主体源文件仍在 CMake 清单中；PowerShell 脚本语法检查通过；文档不依赖已归档内容。主体测试开关独立为 `SIDEBAR_BUILD_TESTS`，默认不构建或运行具体插件测试。

首轮全新 Ninja 构建及 26 项回归曾通过，但发现本机中文 MSVC 的 `/showIncludes` 前缀在 CMake/Ninja 缓存中乱码，可能漏记头文件依赖。因此该缓存仅保留为诊断，不作为后续增量构建交付入口。已将默认入口改为 VS2022 x64，Ninja 后备必须通过英文前缀核验；没有修改系统语言、安装语言包或清除旧缓存。

## 人工验收与发布边界

- 自动回归只验证其覆盖的模型、协议、mock 和画布，不代表真实 Wi-Fi/蓝牙切换、USB 弹出或安装身份能力均通过。
- 依赖包身份的通知、OCR、热点需另行签名安装后验收；本轮未签名、安装、信任或创建证书，也没有操作真实设备。
- 保持唤醒仍只用模拟接口验收；全屏/DPI/多显示器、长期占用、非空通知、真实媒体会话及新布局手感仍需人工确认。
- 预览 EXE 可由用户手动启动，但会使用本用户的应用设置；不要把直接 EXE 当作完整 MSIX 功能验收。
- 主体代码尚未声明对外发布许可证；Microsoft 图标许可证已保留，不擅自为项目主体选择授权方式。

前一阶段交付的源码快照、逐文件清单及测试证据放在仓库外 `work/reviews/host-minimal-20261016/`；源码 ZIP 不包含 `.git` 历史、构建文件、旧材料、日志、私钥或用户数据。该 ZIP 是审核时快照，后续提交说明更新以当前 Git 树为准。真实 Git 仓库始终留在原路径；旧文件仍可从 Git 历史或本地归档恢复。

## 合并与静态检查流程

用户已授权按顺序将审核后的主体源码提交到 `codex-do`、合并到 `main`、运行静态检查、通过后正常推送 `origin/main`。保留全部历史，不强推、不修改远程其他分支，不创建新安装包或 Git 标签。

静态检查覆盖：Git 补丁空白、PowerShell 语法、CMake 全新生成及文件依赖、文档链接、版本一致性、敏感文件/高风险密钥特征以及 QML 静态诊断；人工复核构建/插件/布局边界。静态检查不等同于真实硬件或 MSIX 功能验收；上文 26 项回归是另有证据的动态测试结果。

### 2026-10-16 主分支合并检查结果

主体精简提交为 `1005eb0`，已从 `codex-do` 快进合并到 `main`，无冲突，不改写历史。下列检查在合并后完成；本报告提交后按用户授权正常推送 `origin/main`，不推送其他分支或标签。

| 项目 | 结果 |
| --- | --- |
| Git 补丁空白 | `git diff --check origin/main..HEAD` 通过 |
| PowerShell 脚本 | 5 个脚本在 Windows PowerShell 5.1.26100.9711 和 PowerShell 7.6.6 中均无语法错误；仅解析，不执行安装脚本 |
| 全新 CMake 生成 | VS2022 x64、Qt 6.9.3 配置及生成成功，主体测试开启、真实设备辅助测试关闭；可选 Vulkan 头文件仍缺失，不阻断生成 |
| 文件清单与文档 | `src/`、`include/`、`qml/` 的所有现存源码均在 CMake 清单内；78 个本地文档文件链接均存在 |
| 版本一致性 | CMake / MSIX / EXE 资源版本保持 `0.8.2.0`，运行标识保持 `0.8.2.0-preview` |
| 候选敏感内容 | 当前候选树未检出私钥/令牌高风险特征、私钥证书包或实际用户配置；待推送对象中无超过 10 MB 的单个文件。特征扫描不是完整历史安全认证 |
| 人工源码复核 | 通用插件进程 / JSON / 单活动版本、卡片布局持久化、构建部署边界未发现可确认的 P1/P2 阻断项 |
| 合并后自动回归 | 使用本轮同源 VS 构建产物重跑 26 项隔离 / mock / 离屏回归，26/26 通过，27.06 秒；未操作真实设备 |

Qt 6.9.3 `qmllint` 对全部 36 个 QML / JS 文件报告 **0 个 error、1419 个 warning、2 个 info**，不是零警告通过。警告分类为：1357 个 `unqualified`、60 个 `missing-property`、1 个 `duplicate-property-binding`、1 个 `use-proper-function`；另有 2 个未使用 import 提示。原始 JSON 完整保留，没有关闭警告类别来降低计数。

主要技术债是宿主 context property、动态 Loader / delegate 和宽泛 QObject / QQuickItem 类型缺少显式静态类型；应后续逐步改为显式属性 / 类型接口，不能仅凭工具退出码 0 判断这些访问都正确。`Main.qml` 的 callback 为测试截图延迟回调；`TopIslandHost.qml` 的透明度初值和 `SequentialAnimation on opacity` 经人工核对，是初值加属性动画的用法，[Qt 官方也提供同类示例](https://doc.qt.io/qt-6.8/qtquick-statesanimations-animations.html#using-predefined-targets-and-properties)，未确认为运行缺陷。相关界面回归本轮通过，但不代表所有动态运行路径均已覆盖。

本次没有运行 clang-tidy / cppcheck，也不声称进行了完整 C++ 静态分析或安全认证。检查日志、QML JSON 和本轮 JUnit 在仓库外 `work/reviews/host-publish-20261016/`。本次不增加安装包、不修改版本、不重新安装应用；仍按上文边界保留人工硬件验收事项。
