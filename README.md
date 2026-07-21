# Windows Sidebar

使用 Qt/QML 和 C++20 编写的 Windows 侧边栏。

## Windows 通知权限

Windows 通知监听依赖应用包身份。直接运行构建目录中的
`appqt_rainmeter.exe` 无法获得该身份。

生成已签名的开发 MSIX：

```powershell
cmake --build <构建目录> --target package_msix
```

安装 MSIX：

```powershell
cmake --build <构建目录> --target install_msix
```

首次安装时需要接受 Windows 的本地开发证书安全提示。安装完成后请从
“开始”菜单启动 **Windows Sidebar**，并在首次启动时允许通知访问。
