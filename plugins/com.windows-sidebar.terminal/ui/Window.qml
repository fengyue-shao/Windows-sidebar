import QtQuick
import QtQuick.Window

Window {
    width: 960; height: 760; minimumWidth: 640; minimumHeight: 520
    visible: true; color: themeManager.surface; title: "Windows Sidebar · 受限终端（独立插件）"
    Main { anchors.fill: parent; anchors.margins: 16 }
    onClosing: terminalManager.stop()
}
