import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: systemTrayManager.batteryPresent ? 188 : 164
    radius: 16
    color: themeManager.surface
    border.width: 1
    border.color: themeManager.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 9
            Rectangle {
                Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 10
                color: muteHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
                Text { anchors.centerIn: parent; text: systemTrayManager.muted || systemTrayManager.volume === 0 ? "×" : "♪"; color: systemTrayManager.muted ? themeManager.danger : themeManager.accent; font.pixelSize: 15; font.weight: Font.Bold }
                HoverHandler { id: muteHover }
                TapHandler { onTapped: systemTrayManager.toggleMuted() }
                ToolTip.visible: muteHover.hovered
                ToolTip.text: systemTrayManager.muted ? "取消静音" : "静音"
            }
            Slider {
                Layout.fillWidth: true
                from: 0; to: 100; stepSize: 1
                value: systemTrayManager.volume
                onMoved: systemTrayManager.setVolume(Math.round(value))
            }
            Text {
                Layout.preferredWidth: 34
                horizontalAlignment: Text.AlignRight
                text: systemTrayManager.volume
                color: themeManager.textSecondary
                font.pixelSize: 10
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 7
            Text { text: systemTrayManager.online ? "●" : "○"; color: systemTrayManager.online ? themeManager.success : themeManager.danger; font.pixelSize: 9 }
            Text { Layout.fillWidth: true; text: systemTrayManager.online ? "已连接网络" : "网络未连接"; color: themeManager.textMuted; font.pixelSize: 9 }
            Text { text: systemTrayManager.audioDeviceName; color: themeManager.textFaint; font.pixelSize: 8; elide: Text.ElideRight; Layout.maximumWidth: 170 }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            QuickTile { Layout.fillWidth: true; symbol: "♪"; label: "声音"; onClicked: systemTrayManager.openSoundSettings() }
            QuickTile { Layout.fillWidth: true; symbol: "◎"; label: "网络"; onClicked: systemTrayManager.openNetworkSettings() }
            QuickTile { Layout.fillWidth: true; symbol: "ᛒ"; label: "蓝牙"; onClicked: systemTrayManager.openBluetoothSettings() }
            QuickTile { Layout.fillWidth: true; symbol: "▣"; label: "显示"; onClicked: systemTrayManager.openDisplaySettings() }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: systemTrayManager.batteryPresent
            Text { text: systemTrayManager.charging ? "⚡" : "▰"; color: systemTrayManager.charging ? themeManager.success : themeManager.textSecondary; font.pixelSize: 11 }
            Text { Layout.fillWidth: true; text: systemTrayManager.charging ? "电池正在充电" : "电池电量"; color: themeManager.textMuted; font.pixelSize: 10 }
            Text { text: systemTrayManager.batteryPercent + "%"; color: themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold }
        }
    }

    component QuickTile: Rectangle {
        id: tile
        property string symbol
        property string label
        signal clicked()
        implicitHeight: 48
        radius: 12
        color: tileHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
        border.width: 1
        border.color: tileHover.hovered ? themeManager.accent : themeManager.border
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: tile.symbol; color: themeManager.accent; font.pixelSize: 12 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: tile.label; color: themeManager.textSecondary; font.pixelSize: 8 }
        }
        HoverHandler { id: tileHover }
        TapHandler { onTapped: tile.clicked() }
    }
}
