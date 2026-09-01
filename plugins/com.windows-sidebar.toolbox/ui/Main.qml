import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text { text: "插件工具箱"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text { text: "这是由 plugin.json 动态注册的页面"; color: themeManager.textMuted; font.pixelSize: 10 }
            }
            Rectangle {
                Layout.preferredWidth: 54; Layout.preferredHeight: 26; radius: 10
                color: themeManager.accentSoft
                Text { anchors.centerIn: parent; text: "PLUGIN"; color: themeManager.accent; font.pixelSize: 8; font.weight: Font.Bold }
            }
        }

        Repeater {
            model: [
                { symbol: "⌘", title: "命令面板", detail: "未来在这里接入受限终端 Runner", state: "规划中" },
                { symbol: "▣", title: "插件存储", detail: "每个插件拥有独立数据目录", state: "原型" },
                { symbol: "◇", title: "权限中心", detail: "文件、网络与进程权限由宿主授权", state: "待实现" }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: 78; radius: 16
                color: cardHover.hovered ? themeManager.surfaceHover : themeManager.surface
                border.width: 1; border.color: themeManager.border
                RowLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 11
                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 38; radius: 12; color: themeManager.accentSoft
                        Text { anchors.centerIn: parent; text: modelData.symbol; color: themeManager.accent; font.pixelSize: 14 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Text { text: modelData.title; color: themeManager.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
                        Text { Layout.fillWidth: true; text: modelData.detail; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
                    }
                    Text { text: modelData.state; color: themeManager.textFaint; font.pixelSize: 8 }
                }
                HoverHandler { id: cardHover }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 92; radius: 16
            color: themeManager.overlayStart; border.width: 1; border.color: themeManager.border
            Column {
                anchors.centerIn: parent; spacing: 6
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "动态插件页加载成功"; color: themeManager.success; font.pixelSize: 11; font.weight: Font.DemiBold }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "移除插件目录并重载后，此页面将从导航消失"; color: themeManager.textMuted; font.pixelSize: 9 }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
