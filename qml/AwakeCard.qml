import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

CardSurface {
    id: root
    property bool confirming: false
    implicitHeight: content.implicitHeight + 32
    onVisibleChanged: if (!visible) confirming = false
    ColumnLayout {
        id: content
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 10
        Text { text: "定时保持唤醒"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
        Text { Layout.fillWidth: true; text: awakeManager.status + (awakeManager.active ? " · 剩余 " + Math.ceil(awakeManager.remainingSeconds / 60) + " 分钟" : ""); color: themeManager.textMuted; font.pixelSize: 11; wrapMode: Text.Wrap }
        RowLayout {
            Basic.ComboBox { id: duration; Layout.fillWidth: true; model: ["15 分钟", "30 分钟", "60 分钟", "120 分钟", "240 分钟"]; enabled: !awakeManager.active; palette.buttonText: themeManager.textPrimary; palette.text: themeManager.textPrimary; palette.base: themeManager.surfaceStrong; palette.button: themeManager.surfaceStrong; palette.window: themeManager.surfaceStrong }
            CardAction { text: awakeManager.active ? "停止" : "开始"; onClicked: { if (awakeManager.active) { awakeManager.stop(); root.confirming = false } else root.confirming = true } }
        }
        Basic.CheckBox {
            id: display; text: "同时保持屏幕开启"; enabled: !awakeManager.active; implicitHeight: 28
            indicator: Rectangle {
                width: 18; height: 18; y: (display.height - height) / 2; radius: 5
                color: display.checked ? themeManager.accent : themeManager.surfaceStrong
                border.width: 1; border.color: display.activeFocus ? themeManager.accent : themeManager.border
                Text { anchors.centerIn: parent; text: display.checked ? "✓" : ""; color: "white"; font.pixelSize: 12 }
            }
            contentItem: Text { text: display.text; leftPadding: 26; verticalAlignment: Text.AlignVCenter; color: themeManager.textPrimary; font.pixelSize: 11 }
        }
        ColumnLayout {
            visible: root.confirming; Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "确认暂时阻止自动睡眠？耗电可能增加，手动睡眠仍可使用。"; color: themeManager.warning; font.pixelSize: 11; wrapMode: Text.Wrap }
            RowLayout {
                CardAction { text: "确认开始"; onClicked: { awakeManager.start([15, 30, 60, 120, 240][duration.currentIndex], display.checked); root.confirming = false } }
                CardAction { text: "取消"; onClicked: root.confirming = false }
            }
        }
        Text { Layout.fillWidth: true; text: "到期、停止或退出应用时释放请求；重启后不会自动恢复。"; color: themeManager.textFaint; font.pixelSize: 10; wrapMode: Text.Wrap }
    }
}
