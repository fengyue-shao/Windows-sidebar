import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "CardRegions.js" as Regions

ColumnLayout {
    id: root
    property var model
    property bool popupOpen: false
    spacing: 16
    function closePopups() { for (const item of Regions.delegates(listView)) item.closeMenu() }
    function refreshPopupState() {
        popupOpen = Regions.delegates(listView).some(item => item.popupOpen)
    }
    function inputRegions(target) {
        return Regions.collect([toolbar, empty], target, root).concat(Regions.collect(Regions.delegates(listView), target, listView))
    }
    CardSurface {
        id: toolbar
        Layout.fillWidth: true; Layout.preferredHeight: 70
        RowLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 12
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { text: "通知中心"; color: themeManager.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text { text: root.model ? root.model.unreadCount + " 条未读" : "暂无未读"; color: themeManager.textMuted; font.pixelSize: 10 }
            }
            HeaderButton { text: "全部已读"; enabled: root.model && root.model.unreadCount > 0; onClicked: root.model.markAllAsRead() }
            HeaderButton { text: "清空"; destructive: true; enabled: listView.count > 0; onClicked: root.model.clear() }
        }
    }
    CardSurface {
        id: empty
        Layout.fillWidth: true; Layout.preferredHeight: 112; visible: listView.count === 0
        Column { anchors.centerIn: parent; spacing: 8
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "✓  暂无通知"; color: themeManager.textSecondary; font.pixelSize: 14 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "新的通知会以独立卡片显示在这里"; color: themeManager.textMuted; font.pixelSize: 11 }
        }
    }
    ListView {
        id: listView
        Layout.fillWidth: true; Layout.fillHeight: true
        spacing: 12; clip: true; model: root.model; boundsBehavior: Flickable.StopAtBounds
        onCountChanged: Qt.callLater(root.refreshPopupState)
        delegate: NotificationItem {
            width: listView.width
            appName: model.appName; title: model.title; content: model.content; appIcon: model.appIcon
            unread: model.unread; removable: model.removable; timeText: Qt.formatTime(model.timestamp, "HH:mm")
            onPopupOpenChanged: root.refreshPopupState()
            Component.onDestruction: Qt.callLater(root.refreshPopupState)
            onRemoveRequested: root.model.removeNotification(index)
            onMarkReadRequested: root.model.markAsRead(index)
            onMarkUnreadRequested: root.model.markAsUnread(index)
        }
    }
    component HeaderButton: Basic.Button {
        property bool destructive: false
        implicitWidth: contentItem.implicitWidth + 20; implicitHeight: 34
        opacity: enabled ? 1 : 0.45
        contentItem: Text { text: parent.text; color: parent.destructive ? themeManager.danger : themeManager.textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: parent.hovered ? (parent.destructive ? themeManager.dangerSoft : themeManager.surfaceHover) : themeManager.surfaceStrong }
    }
}
