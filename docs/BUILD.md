# 主应用构建与测试

本仓库保留 Windows Sidebar 主应用和通用插件管理 / JSON 协议，不依赖终端插件源码、Runner 或终端 UI。本轮只整理仓库，源码版本保持 **0.8.2.0**；运行时仍为预览版本，不因仓库精简而升版。

## 开发依赖

- Windows 10 / 11 x64。
- Qt **6.8 或更新的 Qt 6**，使用 MSVC x64 桌面套件；需要 Core、Gui、Quick / QML、Quick Controls、Svg、Network。运行自动测试还需 Qt Test。不要混用 MinGW、ARM64、x86 或其他 Qt 版本的 DLL。
- 支持 C++20 的 MSVC x64 工具链，建议 Visual Studio 2022 的“使用 C++ 的桌面开发”工作负载，以及 Windows 10 / 11 SDK。
- **CMake 3.21 或更新版本**及同安装目录的 CTest，用于本构建入口的 Visual Studio 2022 / CTest 命令支持。源码自身的最低 CMake 版本见根 `CMakeLists.txt`；建议使用 Qt 套件提供的 CMake 或较新的稳定版本。
- **默认优先 Visual Studio 2022 x64 生成器**。没有 VS 2022 时才使用本机已有 Ninja 作为后备，并要求 MSVC 的英文 `/showIncludes` 前缀能够正确识别。脚本不会下载或安装工具 / 语言包。
- Windows PowerShell 5.1 或 PowerShell 7。脚本不更改执行策略；如本机策略阻止运行，应按自己的开发环境政策处理。

MSIX 安装包额外需要 Windows SDK 的 MakeAppx / SignTool 以及匹配清单发布者的签名证书。这些不属于普通编译的必要步骤。

## 安全构建入口

在仓库根目录执行：

```powershell
.\scripts\build.ps1 -QtBin 'D:\Qt\6.9.3\msvc2022_64\bin' -CMake 'D:\cmake\bin\cmake.exe'
```

这里的路径只是当前开发机示例，并非其他电脑的固定要求。也可省略参数，让脚本查找 `QTDIR`、PATH 和常见本地安装位置。无法发现依赖会报错，不会自动安装。

默认行为仅配置并编译 `appqt_rainmeter`，不会部署 Qt 运行库、打包、安装、信任 / 创建证书、关闭运行中的应用或启动软件。默认构建目录是仓库的同级 `build-host`，构建产物不进入源码仓库。

```powershell
# 指定构建目录和并发数。
.\scripts\build.ps1 -QtBin 'D:\Qt\6.9.3\msvc2022_64\bin' -BuildDir '..\build-host-release' -Parallel 4

# 编译并运行主应用自动回归；不运行真实硬件/系统变更测试。
.\scripts\build.ps1 -QtBin 'D:\Qt\6.9.3\msvc2022_64\bin' -BuildDir '..\build-host-test' -Test

# 显式部署 Release Qt 运行库到可执行文件所在构建目录，便于本地预览。
.\scripts\build.ps1 -QtBin 'D:\Qt\6.9.3\msvc2022_64\bin' -BuildDir '..\build-host-preview' -Test -Deploy

# Debug 使用独立构建目录；现有部署脚本只支持 Release，不能搭配 -Deploy。
.\scripts\build.ps1 -QtBin 'D:\Qt\6.9.3\msvc2022_64\bin' -BuildDir '..\build-host-debug' -Configuration Debug -Test
```

脚本通过 `vswhere` 或当前 VS 开发环境发现 MSVC，再导入 `vcvars64.bat`。环境变量仅在当前进程临时调整，并在脚本结束时恢复；不会写入系统或用户的持久环境变量。Visual Studio 生成器的程序一般位于 `<BuildDir>\Release\appqt_rainmeter.exe`；Ninja 的程序位于 `<BuildDir>\appqt_rainmeter.exe`。实际位置由脚本输出。

路径和缓存保护：

- 不允许在源码目录、其子目录、其父目录或盘符根目录构建；构建路径不能通过目录联接 / 符号链接间接指向其他位置。
- 已有缓存必须属于当前源码目录，且 Qt、编译器、x64 平台和生成器匹配。不兼容的缓存会被拒绝；请选择新的构建目录。
- Ninja 是单配置生成器，Release / Debug 使用不同构建目录。
- Ninja 配置后必须通过 `CMakeCXXCompiler.cmake` 的英文 `Note: including file: ` 前缀核验；已有缓存也在重新配置前核验。前缀缺失、乱码或其他语言时会停止，防止遗漏头文件依赖。请选择新的 VS 2022 构建目录，或自行确认编译器语言资源匹配；脚本不会改变系统语言、自动安装语言包或清理缓存，也不假定设置 `VSLANG=1033` 就能修复缺少英文资源的问题。
- 非空、无 CMake 缓存的目录不会被接管。旧终端构建缓存不会复用。
- 没有 `-Clean` 参数，也不会递归删除任何已有构建目录。清理旧产物需自行确认目标和保留内容后另行处理。

`-Deploy` 仅调用 `deploy_runtime`：使用 `windeployqt` 部署主应用所需 Qt / QML，并校验本地 `qt.conf` 和运行库。它不捆绑任何插件。目标预览程序正在运行时会报错，请自行关闭对应预览实例后重试；不会关闭其他应用。此步骤不等于 MSIX 打包，也不能替代 OCR、热点、通知权限等已安装应用能力的验收。

## 测试开关与证据

`-Test` 配置 `SIDEBAR_BUILD_TESTS=ON`，编译测试目标并执行 CTest；默认该选项为 `OFF`。脚本始终配置 `SIDEBAR_BUILD_INTEGRATION_TESTS=OFF`。注册到默认 CTest 的用例不实际切换 Wi-Fi / 蓝牙、开启热点、弹出 USB、修改用户通知或保持唤醒设置。它们验证模型、布局、协议和模拟接口等主应用逻辑。

`SIDEBAR_BUILD_INTEGRATION_TESTS=ON` 仅供专门的手动验收流程构建辅助程序；不代表获得真实系统操作授权，也不会把这些辅助程序自动加入默认 CTest。普通构建不需要开启。

失败时保留构建和测试证据，不自动清理。CTest 的详情位于 `<BuildDir>\Testing\Temporary\LastTest.log`。自动回归通过只表示当前回归覆盖范围通过，不能据此宣称所有硬件、安装包权限或实机界面已验收。

如需直接使用 CMake，先打开 **x64 Native Tools Command Prompt for VS 2022**，确保 `cmake`、`ctest` 和匹配套件的 Qt DLL 在 PATH：

```powershell
cmake -S . -B ..\build-host-manual -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_PREFIX_PATH=D:\Qt\6.9.3\msvc2022_64' -DSIDEBAR_BUILD_TESTS=ON -DSIDEBAR_BUILD_INTEGRATION_TESTS=OFF
cmake --build ..\build-host-manual --config Release --parallel 2
ctest --test-dir ..\build-host-manual -C Release --output-on-failure --no-tests=error
```

## 运行与安装边界

本地预览需先显式 `-Deploy`，再由你手动运行脚本输出的 EXE；不会自动启动另一个同名主应用。普通 EXE 与已安装 MSIX 的应用身份 / 权限不同，涉及这些权限的功能需在安装包中另行验收。

需要交付安装包时，先审核 `packaging` 清单和 `scripts/build-msix.ps1` 的参数与行为，再由你明确决定签名和安装流程。打包脚本默认要求已有可用证书；缺少证书会停止，创建开发证书必须由用户显式选择 `-CreateDevelopmentCertificate`。开发签名证书的创建、私钥保管、证书信任和替换已有应用均是独立的用户决策，**不由 `build.ps1` 自动执行**。不要把测试证书私钥 / PFX / 密码提交进仓库或随测试包公开分发。

`scripts/install-msix.ps1` 保留为显式人工安装辅助工具；不要将它作为普通构建的后续隐式步骤。是否信任测试证书、是否创建新的开发证书、是否安装 / 替换本机应用，需要先确认。发布给朋友测试前，还应核对实际包版本、签名发布者、依赖、升级行为和已通过 / 未通过的功能验收清单。
