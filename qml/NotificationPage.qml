import QtQuick
import QtQuick.Layouts
import "CardRegions.js" as Regions

Item {
    id: root
    readonly property bool popupOpen: notices.popupOpen
    function closePopups() { notices.closePopups() }
    function inputRegions(target) {
        return Regions.collect([disabled], target, root).concat(notices.visible ? notices.inputRegions(target) : [])
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 16
        NotificationArea { id: notices; Layout.fillWidth: true; Layout.fillHeight: true; visible: uiSettings.notificationsEnabled; model: notificationModel; viewModel: notificationViewModel; service: notificationListener }
        CardSurface {
            id: disabled
            visible: !uiSettings.notificationsEnabled
            Layout.fillWidth: true; Layout.preferredHeight: 96
            Text { anchors.centerIn: parent; text: "通知模块已关闭，可在顶部设置中开启。"; color: themeManager.textMuted; font.pixelSize: 12 }
        }
        Item { Layout.fillHeight: true; visible: !notices.visible }
    }
}
