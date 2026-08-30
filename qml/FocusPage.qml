import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    readonly property int minutes: Math.floor(focusTimer.remainingSeconds / 60)
    readonly property int seconds: focusTimer.remainingSeconds % 60
    readonly property string timeText: String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0")

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "专注空间"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
            Rectangle {
                Layout.preferredWidth: sessionText.implicitWidth + 18; Layout.preferredHeight: 26; radius: 13
                color: themeManager.accentSoft
                Text { id: sessionText; anchors.centerIn: parent; text: "已完成 " + focusTimer.completedSessions; color: themeManager.accent; font.pixelSize: 10; font.weight: Font.DemiBold }
                TapHandler { onTapped: focusTimer.clearCompletedSessions() }
                ToolTip.visible: sessionHover.hovered
                ToolTip.text: "点击清零完成次数"
                HoverHandler { id: sessionHover }
            }
        }

        Text {
            Layout.fillWidth: true
            text: focusTimer.running ? "把注意力留给眼前这一件事" : "选择节奏，然后开始"
            color: themeManager.textMuted
            font.pixelSize: 11
        }

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 230
            Layout.preferredHeight: 230

            Canvas {
                id: ring
                anchors.fill: parent
                property real progress: focusTimer.progress
                property color trackColor: themeManager.track
                property color progressColor: themeManager.accent
                onProgressChanged: requestPaint()
                onTrackColorChanged: requestPaint()
                onProgressColorChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    const center = width / 2
                    const radius = center - 12
                    ctx.reset()
                    ctx.lineWidth = 10
                    ctx.lineCap = "round"
                    ctx.strokeStyle = trackColor
                    ctx.beginPath()
                    ctx.arc(center, center, radius, 0, Math.PI * 2)
                    ctx.stroke()
                    if (progress > 0) {
                        ctx.strokeStyle = progressColor
                        ctx.beginPath()
                        ctx.arc(center, center, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * progress)
                        ctx.stroke()
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 5
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.timeText; color: themeManager.textPrimary; font.pixelSize: 43; font.weight: Font.Light }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: focusTimer.running ? "进行中" : "准备就绪"; color: focusTimer.running ? themeManager.success : themeManager.textMuted; font.pixelSize: 10 }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            PresetButton { Layout.fillWidth: true; label: "短休息"; minutes: 5 }
            PresetButton { Layout.fillWidth: true; label: "专注"; minutes: 25 }
            PresetButton { Layout.fillWidth: true; label: "深度"; minutes: 50 }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ActionButton { Layout.fillWidth: true; primary: true; label: focusTimer.running ? "暂停" : "开始"; onClicked: focusTimer.toggle() }
            ActionButton { Layout.preferredWidth: 92; label: "重置"; onClicked: focusTimer.reset() }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            radius: 16
            color: themeManager.surface
            border.width: 1
            border.color: themeManager.border
            Column {
                anchors.centerIn: parent
                spacing: 5
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "完成一个周期后，计数会自动累积"; color: themeManager.textSecondary; font.pixelSize: 10 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "计时由后台 C++ 对象维护，切换页面不会中断"; color: themeManager.textFaint; font.pixelSize: 9 }
            }
        }

        Item { Layout.fillHeight: true }
    }

    component PresetButton: Rectangle {
        id: preset
        property string label
        property int minutes
        implicitHeight: 42
        radius: 13
        color: focusTimer.durationSeconds === minutes * 60 ? themeManager.accentSoft : presetHover.hovered ? themeManager.surfaceHover : themeManager.surface
        border.width: 1
        border.color: focusTimer.durationSeconds === minutes * 60 ? themeManager.accent : themeManager.border
        Text { anchors.centerIn: parent; text: preset.label + " · " + preset.minutes; color: focusTimer.durationSeconds === preset.minutes * 60 ? themeManager.accent : themeManager.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold }
        HoverHandler { id: presetHover }
        TapHandler { onTapped: focusTimer.setDurationMinutes(preset.minutes) }
    }

    component ActionButton: Rectangle {
        id: action
        property string label
        property bool primary: false
        signal clicked()
        implicitHeight: 48
        radius: 15
        color: primary ? (actionTap.pressed ? themeManager.accentHover : themeManager.accent) : (actionHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong)
        border.width: primary ? 0 : 1
        border.color: themeManager.borderStrong
        Text { anchors.centerIn: parent; text: action.label; color: action.primary ? "#FFFFFF" : themeManager.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
        HoverHandler { id: actionHover }
        TapHandler { id: actionTap; onTapped: action.clicked() }
    }
}
