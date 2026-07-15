import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {

    id: root

    property var model

    spacing: 10
    width:0.9*parent.width
    height:500

    RowLayout {

        Layout.fillWidth: true

        Text {

            text: "Notifications"

            color: "white"

            font.pixelSize: 18

            font.bold: true

            Layout.fillWidth: true
        }

        ToolButton {
            text: "✓"

            onClicked:
                notificationModel.markAllAsRead()

            ToolTip.text: "全部已读"

            ToolTip.visible: hovered
        }

        ToolButton {

            text: "🗑"

            onClicked:
                notificationModel.clear()

            ToolTip.text: "清空通知"

            ToolTip.visible: hovered
        }
    }

    Rectangle {

        Layout.fillWidth: true
        Layout.fillHeight: true

        radius: 16

        color: "#12FFFFFF"

        ListView {

            id: listView

            anchors.fill: parent

            anchors.margins: 8

            spacing: 8

            clip: true

            model: root.model

            delegate: NotificationItem {

                width: listView.width

                appName: model.appName
                title: model.title
                content: model.content
                appIcon: model.appIcon
                unread: model.unread

                timeText: Qt.formatTime(model.timestamp, "hh:mm")

                onRemoveRequested: root.model.removeNotification(index)

                onMarkReadRequested: root.model.markAsRead(index)

                onMarkUnreadRequested: root.model.markAsUnread(index)
            }

            ScrollBar.vertical: ScrollBar {

                policy: ScrollBar.AsNeeded
                visible:false
            }
        }

        Text {

            anchors.centerIn: parent

            visible: listView.count === 0

            text: "暂无通知"

            color: "#80FFFFFF"

            font.pixelSize: 14
        }
    }
}