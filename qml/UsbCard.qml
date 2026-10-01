import QtQuick
import QtQuick.Layouts

CardSurface {
    id: root
    property string pendingId: ""
    property string pendingName: ""
    property var deviceItems: usbManager.devices
    implicitHeight: content.implicitHeight + 32
    onVisibleChanged: if (!visible) pendingId = ""
    ColumnLayout {
        id: content
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "USB 安全弹出"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
            CardAction { text: "刷新"; enabled: !usbManager.busy; onClicked: { root.pendingId = ""; usbManager.refresh() } }
        }
        Text { Layout.fillWidth: true; text: usbManager.status; color: themeManager.textMuted; wrapMode: Text.Wrap; font.pixelSize: 11; textFormat: Text.PlainText }
        Repeater {
            model: root.deviceItems
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: modelData.name + "\n" + modelData.volumes; color: themeManager.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                CardAction { text: "弹出"; enabled: !usbManager.busy; onClicked: { root.pendingId = modelData.id; root.pendingName = modelData.name + " · " + modelData.volumes } }
            }
        }
        ColumnLayout {
            visible: root.pendingId.length > 0; Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "确认安全弹出整个设备？\n" + root.pendingName + "\n该设备上的所有卷都将断开，请先保存并关闭文件。"; color: themeManager.warning; font.pixelSize: 11; wrapMode: Text.Wrap; textFormat: Text.PlainText }
            RowLayout {
                CardAction { text: "确认弹出"; enabled: !usbManager.busy; onClicked: { usbManager.eject(root.pendingId); root.pendingId = "" } }
                CardAction { text: "取消"; onClicked: root.pendingId = "" }
            }
        }
        Text { Layout.fillWidth: true; text: "仅显示可核验的 USB 外接存储；系统盘、启动盘及未知设备不提供弹出。"; color: themeManager.textFaint; font.pixelSize: 10; wrapMode: Text.Wrap }
    }
}
