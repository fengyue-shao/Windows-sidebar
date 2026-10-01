import QtQuick
import QtQuick.Layouts
import "CardRegions.js" as Regions

Item {
    id: root
    readonly property bool popupOpen: notices.popupOpen
    function closePopups() { notices.closePopups() }
    function inputRegions(target) {
        return Regions.collect([media, mediaEmpty, disabled], target, root).concat(notices.visible ? notices.inputRegions(target) : [])
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 16
        MediaBar { id: media; Layout.fillWidth: true; Layout.preferredHeight: 152; moduleEnabled: uiSettings.mediaEnabled }
        CardSurface {
            id: mediaEmpty
            visible: uiSettings.mediaEnabled && !media.visible
            Layout.fillWidth: true; Layout.preferredHeight: 72
            Column { x: 18; y: 15; spacing: 7
                Text { text: "正在播放"; color: themeManager.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: "播放媒体后，这里会显示独立的播放控制卡片。"; color: themeManager.textMuted; font.pixelSize: 11 }
            }
        }
        NotificationArea { id: notices; Layout.fillWidth: true; Layout.fillHeight: true; visible: uiSettings.notificationsEnabled; model: notificationModel }
        CardSurface {
            id: disabled
            visible: !uiSettings.notificationsEnabled
            Layout.fillWidth: true; Layout.preferredHeight: 96
            Text { anchors.centerIn: parent; text: "通知模块已关闭，可在顶部设置中开启。"; color: themeManager.textMuted; font.pixelSize: 12 }
        }
        Item { Layout.fillHeight: true; visible: !notices.visible }
    }
}
