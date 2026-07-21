import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string appName: ""
    property string title: ""
    property string content: ""
    property url appIcon
    property bool unread: true
    property bool removable: true
    property string timeText: ""

    signal removeRequested()
    signal markReadRequested()
    signal markUnreadRequested()
    signal clicked()

    implicitHeight: content.length > 0 ? 94 : 76

    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            root.clicked()
            if (root.unread)
                root.markReadRequested()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 13
        color: hover.hovered ? "#20FFFFFF"
                             : root.unread ? "#16FFFFFF" : "#0DFFFFFF"
        border.width: 1
        border.color: hover.hovered ? "#30FFFFFF" : "#12FFFFFF"

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.unread ? 3 : 0
            radius: 2
            color: "#61A9FF"
            Behavior on width { NumberAnimation { duration: 140 } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 11

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                Layout.alignment: Qt.AlignTop
                radius: 11
                color: "#18FFFFFF"

                Text {
                    anchors.centerIn: parent
                    text: root.appName.length > 0 ? root.appName.charAt(0).toUpperCase() : "·"
                    color: "#BFFFFFFF"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Image {
                    id: iconImage
                    anchors.fill: parent
                    anchors.margins: 5
                    source: root.appIcon
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                    asynchronous: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: root.appName || "系统通知"
                        color: "#EFFFFFFF"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.timeText
                        color: "#68FFFFFF"
                        font.pixelSize: 10
                    }

                    Rectangle {
                        visible: root.unread
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: "#61A9FF"
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.title.length > 0
                    text: root.title
                    color: "#D8FFFFFF"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.content.length > 0
                    text: root.content
                    color: "#8EFFFFFF"
                    font.pixelSize: 11
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignTop
                radius: 9
                color: menuHover.hovered ? "#24FFFFFF" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "···"
                    color: "#A0FFFFFF"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                HoverHandler { id: menuHover }
                TapHandler { onTapped: menu.open() }
            }
        }

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Menu {
        id: menu

        MenuItem {
            text: root.unread ? "标记为已读" : "标记为未读"
            onTriggered: root.unread ? root.markReadRequested() : root.markUnreadRequested()
        }

        MenuSeparator { }

        MenuItem {
            text: "从通知中心删除"
            enabled: root.removable
            onTriggered: root.removeRequested()
        }
    }
}
