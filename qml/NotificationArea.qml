import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var model
    spacing: 9

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: "通知"
            color: themeManager.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Rectangle {
            visible: notificationModel.unreadCount() > 0
            implicitWidth: countText.implicitWidth + 14
            implicitHeight: 24
            radius: 12
            color: themeManager.accentSoft

            Text {
                id: countText
                anchors.centerIn: parent
                text: notificationModel.unreadCount()
                color: themeManager.textPrimary
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
        }

        HeaderButton {
            text: "全部已读"
            enabled: notificationModel.unreadCount() > 0
            onClicked: notificationModel.markAllAsRead()
        }

        HeaderButton {
            text: "清空"
            destructive: true
            enabled: listView.count > 0
            onClicked: notificationModel.clear()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        radius: 16
        color: themeManager.surface
        border.width: 1
        border.color: themeManager.border
        clip: true

        ListView {
            id: listView
            anchors.fill: parent
            anchors.margins: 8
            spacing: 7
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds

            delegate: NotificationItem {
                width: listView.width
                appName: model.appName
                title: model.title
                content: model.content
                appIcon: model.appIcon
                unread: model.unread
                removable: model.removable
                timeText: Qt.formatTime(model.timestamp, "HH:mm")
                onRemoveRequested: root.model.removeNotification(index)
                onMarkReadRequested: root.model.markAsRead(index)
                onMarkUnreadRequested: root.model.markAsUnread(index)
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            add: Transition {
                NumberAnimation { properties: "opacity,scale"; from: 0; to: 1; duration: 180 }
            }
            displaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 180; easing.type: Easing.OutCubic }
            }
        }

        Column {
            anchors.centerIn: parent
            visible: listView.count === 0
            spacing: 7

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "✓"
                color: themeManager.accentSoft
                font.pixelSize: 30
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "暂无通知"
                color: themeManager.textMuted
                font.pixelSize: 13
            }
        }
    }

    component HeaderButton: Rectangle {
        id: button

        property string text
        property bool destructive: false
        signal clicked()

        implicitWidth: label.implicitWidth + 16
        implicitHeight: 28
        radius: 9
        color: !enabled ? themeManager.surface
                        : hover.hovered ? (destructive ? themeManager.dangerSoft : themeManager.surfaceHover)
                                        : themeManager.surface
        opacity: enabled ? 1 : 0.45

        Text {
            id: label
            anchors.centerIn: parent
            text: button.text
            color: button.destructive ? themeManager.danger : themeManager.textSecondary
            font.pixelSize: 11
        }

        HoverHandler { id: hover }
        TapHandler { enabled: button.enabled; onTapped: button.clicked() }
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
