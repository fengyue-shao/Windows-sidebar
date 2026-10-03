import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "CardRegions.js" as Regions

ColumnLayout {
    id: root
    property var model
    property var viewModel: null
    property var service: null
    property bool popupOpen: false
    spacing: 16
    function closePopups() { for (const item of Regions.delegates(listView)) item.closeMenu() }
    function refreshPopupState() {
        popupOpen = Regions.delegates(listView).some(item => item.popupOpen)
    }
    function inputRegions(target) {
        return Regions.collect([toolbar, filters, serviceStatus, empty], target, root).concat(Regions.collect(Regions.delegates(listView), target, listView))
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
            HeaderButton { objectName: "notificationMarkVisibleRead"; text: "当前项已读"; enabled: !!root.model && root.model.unreadCount > 0 && listView.count > 0; onClicked: { if (root.viewModel) root.viewModel.markVisibleAsRead(); else root.model.markAllAsRead() } }
            HeaderButton { objectName: "notificationClearVisible"; text: "清空当前项"; destructive: true; enabled: listView.count > 0 && (!root.service || (root.service.active && !root.service.busy)); onClicked: { if (root.viewModel) root.viewModel.clearVisible(); else root.model.clear() } }
        }
    }
    CardSurface {
        id: filters
        visible: root.viewModel !== null
        Layout.fillWidth: true; Layout.preferredHeight: filterColumn.implicitHeight + 24
        ColumnLayout {
            id: filterColumn
            anchors.fill: parent; anchors.margins: 12; spacing: 8
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Basic.TextField {
                    id: searchField
                    objectName: "notificationSearchField"
                    Layout.fillWidth: true
                    implicitHeight: 34
                    placeholderText: "搜索应用、标题或内容"
                    text: root.viewModel ? root.viewModel.searchText : ""
                    color: themeManager.textPrimary
                    placeholderTextColor: themeManager.textMuted
                    selectionColor: themeManager.accent
                    font.pixelSize: 11
                    onTextEdited: if (root.viewModel) root.viewModel.searchText = text
                    background: Rectangle { radius: 10; color: themeManager.surfaceStrong; border.width: 1; border.color: searchField.activeFocus ? themeManager.accent : themeManager.border }
                }
                HeaderButton {
                    objectName: "notificationUnreadFilter"
                    text: "仅未读"
                    checkable: true
                    checked: root.viewModel ? root.viewModel.unreadOnly : false
                    onClicked: if (root.viewModel) root.viewModel.unreadOnly = !root.viewModel.unreadOnly
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: "按应用分组 · 显示 " + listView.count + " 条"; color: themeManager.textMuted; font.pixelSize: 10 }
                HeaderButton {
                    objectName: "notificationResetFilters"
                    text: "重置筛选"
                    visible: root.viewModel && root.viewModel.filtered
                    onClicked: { root.viewModel.searchText = ""; root.viewModel.unreadOnly = false }
                }
            }
        }
    }
    CardSurface {
        id: serviceStatus
        objectName: "notificationServiceStatusCard"
        visible: root.service !== null
        Layout.fillWidth: true
        Layout.preferredHeight: statusRow.implicitHeight + 24
        RowLayout {
            id: statusRow
            anchors.fill: parent; anchors.margins: 12; spacing: 10
            Text {
                objectName: "notificationServiceStatus"
                Layout.fillWidth: true
                text: root.service ? root.service.statusText : ""
                textFormat: Text.PlainText; wrapMode: Text.Wrap
                color: root.service && root.service.failed ? themeManager.warning : themeManager.textMuted
                font.pixelSize: 11
            }
            HeaderButton {
                text: root.service && root.service.active ? "刷新" : "重试"
                enabled: root.service && !root.service.busy
                onClicked: { if (root.service.active) root.service.synchronize(); else root.service.start() }
            }
        }
    }
    CardSurface {
        id: empty
        Layout.fillWidth: true; Layout.preferredHeight: 112; visible: listView.count === 0
        Column { anchors.centerIn: parent; spacing: 8
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.viewModel && root.viewModel.filtered ? "没有符合筛选的通知" : root.service && (!root.service.active || root.service.busy || root.service.failed) ? "通知列表暂为空" : "✓  暂无通知"; color: themeManager.textSecondary; font.pixelSize: 14 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.viewModel && root.viewModel.filtered ? "调整关键词或重置筛选即可查看其他通知" : "新的通知会以独立卡片显示在这里"; color: themeManager.textMuted; font.pixelSize: 11 }
        }
    }
    ListView {
        id: listView
        Layout.fillWidth: true; Layout.fillHeight: true
        objectName: "notificationListView"
        spacing: 12; clip: true; model: root.viewModel || root.model; boundsBehavior: Flickable.StopAtBounds
        section.property: root.viewModel ? "appGroup" : ""
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section
            objectName: "notificationApplicationHeading"
            width: listView.width; height: 28
            text: root.viewModel ? root.viewModel.groupTitle(section) : section
            textFormat: Text.PlainText
            color: themeManager.textSecondary; font.pixelSize: 11; font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }
        onCountChanged: Qt.callLater(root.refreshPopupState)
        delegate: NotificationItem {
            property string noticeId: model.id
            width: listView.width
            appName: model.appName; title: model.title; content: model.content; appIcon: model.appIcon
            unread: model.unread; removable: model.removable && (!root.service || (root.service.active && !root.service.busy)); timeText: Qt.formatTime(model.timestamp, "HH:mm")
            onPopupOpenChanged: root.refreshPopupState()
            Component.onDestruction: Qt.callLater(root.refreshPopupState)
            onRemoveRequested: { if (root.viewModel) root.viewModel.removeNotificationById(noticeId); else root.model.removeNotification(index) }
            onMarkReadRequested: { if (root.viewModel) root.viewModel.markReadById(noticeId, true); else root.model.markAsRead(index) }
            onMarkUnreadRequested: { if (root.viewModel) root.viewModel.markReadById(noticeId, false); else root.model.markAsUnread(index) }
        }
    }
    component HeaderButton: Basic.Button {
        property bool destructive: false
        implicitWidth: contentItem.implicitWidth + 20; implicitHeight: 34
        opacity: enabled ? 1 : 0.45
        contentItem: Text { text: parent.text; color: parent.destructive ? themeManager.danger : themeManager.textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: parent.checked ? themeManager.surfaceHover : parent.hovered ? (parent.destructive ? themeManager.dangerSoft : themeManager.surfaceHover) : themeManager.surfaceStrong; border.width: parent.checked ? 1 : 0; border.color: themeManager.accent }
    }
}
