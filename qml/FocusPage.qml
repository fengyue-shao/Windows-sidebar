import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "CardRegions.js" as Regions

Flickable {
    id: root
    contentWidth: width; contentHeight: grid.implicitHeight
    clip: true; boundsBehavior: Flickable.StopAtBounds
    readonly property bool wide: width >= 620
    readonly property string timeText: String(Math.floor(focusTimer.remainingSeconds/60)).padStart(2, "0") + ":" + String(focusTimer.remainingSeconds%60).padStart(2, "0")
    function inputRegions(target) { return Regions.collect([heading, clock, actions, shortBreak, regular, deep, sessions, continuity], target, root) }
    GridLayout {
        id: grid
        width: root.width; columns: root.wide ? 2 : 1; columnSpacing: 16; rowSpacing: 16
        CardSurface {
            id: heading
            Layout.fillWidth: true; Layout.columnSpan: grid.columns; Layout.preferredHeight: 70
            Column { x: 18; y: 14; spacing: 6
                Text { text: "专注空间"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text { text: focusTimer.running ? "把注意力留给眼前这一件事" : "选择节奏，然后开始"; color: themeManager.textMuted; font.pixelSize: 11 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: (root.width-16)/2; Layout.alignment: Qt.AlignTop; spacing: 16
            FocusClockCard { id: clock; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight; timeText: root.timeText; progress: focusTimer.progress; running: focusTimer.running }
            CardSurface {
                id: actions
                Layout.fillWidth: true; Layout.preferredHeight: 80
                RowLayout { anchors.fill: parent; anchors.margins: 16; spacing: 12
                    Action { Layout.fillWidth: true; text: focusTimer.running ? "暂停" : "开始专注"; primary: true; onClicked: focusTimer.toggle() }
                    Action { Layout.preferredWidth: 90; text: "重置"; onClicked: focusTimer.reset() }
                }
            }
            CardSurface {
                id: continuity
                Layout.fillWidth: true; Layout.preferredHeight: 80
                Column { x: 18; y: 17; spacing: 8
                    Text { text: "跨页面保持计时"; color: themeManager.textSecondary; font.pixelSize: 12 }
                    Text { text: "收起面板或切换卡片，不会中断专注。"; color: themeManager.textMuted; font.pixelSize: 10 }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: (root.width-16)/2; Layout.alignment: Qt.AlignTop; spacing: 16
            FocusPresetCard { id: shortBreak; Layout.fillWidth: true; label: "短休息"; description: "给注意力留一点缓冲"; minutes: 5; selected: focusTimer.durationSeconds === minutes*60; onChosen: focusTimer.setDurationMinutes(minutes) }
            FocusPresetCard { id: regular; Layout.fillWidth: true; label: "标准专注"; description: "适合一个小任务"; minutes: 25; selected: focusTimer.durationSeconds === minutes*60; onChosen: focusTimer.setDurationMinutes(minutes) }
            FocusPresetCard { id: deep; Layout.fillWidth: true; label: "深度工作"; description: "留出完整的思考时间"; minutes: 50; selected: focusTimer.durationSeconds === minutes*60; onChosen: focusTimer.setDurationMinutes(minutes) }
            CardSurface {
                id: sessions
                Layout.fillWidth: true; Layout.preferredHeight: 120
                Text { x: 18; y: 15; text: "已完成周期"; color: themeManager.textSecondary; font.pixelSize: 12 }
                Text { x: 18; y: 40; text: focusTimer.completedSessions; color: themeManager.accent; font.pixelSize: 30 }
                Text { x: 18; y: 92; text: "每完成一个周期，自动累积一次。"; color: themeManager.textMuted; font.pixelSize: 10 }
                Action { anchors.right: parent.right; anchors.rightMargin: 16; y: 34; text: "清零计数"; enabled: focusTimer.completedSessions > 0; onClicked: focusTimer.clearCompletedSessions() }
            }
        }
    }
    component Action: Basic.Button {
        property bool primary: false
        implicitWidth: 92; implicitHeight: 44
        opacity: enabled ? 1 : 0.45
        contentItem: Text { text: parent.text; color: parent.primary ? "white" : themeManager.textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 11; color: parent.primary ? (parent.down ? themeManager.accentHover : themeManager.accent) : (parent.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong); border.width: parent.primary ? 0 : 1; border.color: themeManager.border }
    }
}
