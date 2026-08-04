# 构建与部署

## 环境要求

- Windows 10 2004（10.0.19041）或更高版本；
- Qt 6.8 或更高版本，MSVC 64 位套件；
- Visual Studio 或 Visual Studio Build Tools；
- “使用 C++ 的桌面开发”工作负载；
- Windows SDK；
- CMake；
- PowerShell 5.1 或更高版本。

项目当前使用 C++20，MSIX 清单固定声明 x64 架构。

## 一键编译和部署

在仓库根目录双击：

```text
build-and-deploy.cmd
```

或者在 PowerShell 中运行：

```powershell
.\scripts\build-and-deploy.ps1
```

脚本将自动执行：

1. 查找 Qt、CMake 和 Visual Studio C++ Build Tools；
2. 加载 x64 MSVC 环境；
3. 创建 Release 构建目录；
4. 编译应用；
5. 部署 Qt/QML 依赖；
6. 创建并签名 MSIX；
7. 信任本地开发证书；
8. 安装或更新应用。

常用参数：

```powershell
# Qt 未安装在 C:\Qt 或 D:\Qt 时手动指定
.\scripts\build-and-deploy.ps1 `
  -QtBin "C:\Qt\6.11.1\msvc2022_64\bin"

# 只生成安装包，不安装
.\scripts\build-and-deploy.ps1 -SkipInstall

# 删除项目内的指定构建目录后全量重建
.\scripts\build-and-deploy.ps1 -Clean

# 指定构建目录
.\scripts\build-and-deploy.ps1 `
  -BuildDir ".\build\release-test"
```

默认产物位于：

```text
build/test-machine-release/msix/WindowsSidebar.msix
build/test-machine-release/msix/WindowsSidebarDev.cer
```

## 手动构建

进入已经加载 MSVC 环境的终端后：

```powershell
cmake -S . -B build/release `
  -G "NMake Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\msvc2022_64"

cmake --build build/release --target appqt_rainmeter
```

仅生成签名 MSIX：

```powershell
cmake --build build/release --target package_msix
```

生成并安装 MSIX：

```powershell
cmake --build build/release --target install_msix
```

## 为什么必须安装 MSIX

Windows 通知监听需要应用包身份。直接运行：

```text
build/.../appqt_rainmeter.exe
```

不具备完整包身份，因此通知权限请求、历史通知加载或系统同步可能失败。安装后必须从开始菜单启动 **Windows Sidebar**。

## 开发证书

打包脚本在当前用户证书库中寻找主题为 `CN=WindowsSidebarDev` 的有效私钥证书。若不存在，会创建有效期两年的自签名代码签名证书。

安装脚本会在得到用户确认后，将导出的 `WindowsSidebarDev.cer` 加入当前用户可信根证书库。此证书仅用于开发和测试，不适合正式发布。

## 版本管理

发布新测试版本时需要同步更新：

- `CMakeLists.txt` 中的 `project(... VERSION x.y.z)`；
- `packaging/AppxManifest.xml` 中的四段式 `Version="x.y.z.0"`。

Windows 默认不允许安装同版本但内容不同的包。修改代码并重新分发测试包时，应提高 MSIX 版本号。

## 常见问题

### 0x800B0109：根证书不受信任

使用一键脚本安装，或执行：

```powershell
.\scripts\install-msix.ps1 `
  -PackagePath ".\build\release\msix\WindowsSidebar.msix" `
  -TrustDevelopmentCertificate
```

确认信任提示后重新安装。

### 找不到 Qt

通过 `-QtBin` 指向包含 `qmake.exe` 的 MSVC Qt bin 目录。不要选择 MinGW 套件。

### 找不到 MSVC 或 nmake

在 Visual Studio Installer 中安装或修复“使用 C++ 的桌面开发”，并确保包含 MSVC x64/x86 构建工具和 Windows SDK。

### 构建成功但通知不可用

确认运行的是从开始菜单启动的已安装版本，而不是构建目录里的 exe；同时检查 Windows 是否已授予通知访问权限。

