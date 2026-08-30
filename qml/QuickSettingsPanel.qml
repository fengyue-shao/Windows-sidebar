import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: systemTrayManager.batteryPresent ? 104 : 70
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
            visible: systemTrayManager.batteryPresent
            Text { text: systemTrayManager.charging ? "⚡" : "▰"; color: systemTrayManager.charging ? themeManager.success : themeManager.textSecondary; font.pixelSize: 11 }
            Text { Layout.fillWidth: true; text: systemTrayManager.charging ? "电池正在充电" : "电池电量"; color: themeManager.textMuted; font.pixelSize: 10 }
            Text { text: systemTrayManager.batteryPercent + "%"; color: themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold }
        }
    }
}
