import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string appName
    property string title
    property string content
    property url appIcon
    property bool unread: true
    property string timeText: ""
    property int row: -1

    signal removeRequested()
    signal markReadRequested()
    signal markUnreadRequested()
    signal clicked()

    implicitWidth: 320
    implicitHeight: 80

    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton

        onTapped: {
            root.clicked()

            if (root.unread)
                root.markReadRequested()
            else
                root.markUnreadRequested()
        }
    }

    Rectangle {
        anchors.fill: parent

        radius: 12

        color: hoverHandler.hovered ?
                   "#28FFFFFF"
                 : unread ?
                       "#20FFFFFF"
                     : "#14FFFFFF"

        border.width: hoverHandler.hovered ? 1 : 0
        border.color: "#45FFFFFF"

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }

        Behavior on border.width {
            NumberAnimation {
                duration: 120
            }
        }

        // 左侧未读蓝条
        Rectangle {

            width: unread ? 4 : 0

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            radius: 2

            color: "#4DA3FF"

            Behavior on width {
                NumberAnimation {
                    duration: 120
                }
            }
        }

        RowLayout {

            anchors.fill: parent
            anchors.margins: 12

            spacing: 12

            Image {

                Layout.preferredWidth: 36
                Layout.preferredHeight: 36

                source: appIcon

                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {

                Layout.fillWidth: true

                spacing: 2

                RowLayout {

                    Layout.fillWidth: true

                    Text {

                        Layout.fillWidth: true

                        text: appName

                        color: "white"

                        font.bold: true

                        font.pixelSize: 14

                        elide: Text.ElideRight
                    }

                    Text {

                        text: timeText

                        color: "#AAAAAA"

                        font.pixelSize: 11
                    }

                    // 给菜单按钮预留固定宽度
                    Item {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 1
                    }
                }

                Text {

                    Layout.fillWidth: true

                    text: title

                    color: "#EEEEEE"

                    font.pixelSize: 13

                    elide: Text.ElideRight
                }

                Text {

                    Layout.fillWidth: true

                    text: content

                    color: "#BBBBBB"

                    font.pixelSize: 12

                    maximumLineCount: 2

                    wrapMode: Text.Wrap

                    elide: Text.ElideRight
                }
            }
        }

        ToolButton {

            anchors.top: parent.top
            anchors.right: parent.right

            anchors.topMargin: 6
            anchors.rightMargin: 6

            width: 24
            height: 24

            text: "⋮"

            opacity: hoverHandler.hovered ? 1 : 0
            enabled: hoverHandler.hovered

            Behavior on opacity {
                NumberAnimation {
                    duration: 120
                }
            }

            onClicked: menu.open()
        }

        Menu {

            id: menu

            MenuItem {

                text: unread ? "标记已读" : "标记未读"

                onTriggered: {

                    if (unread)
                        root.markReadRequested()
                    else
                        root.markUnreadRequested()
                }
            }

            MenuItem {

                text: "删除"

                onTriggered:
                    root.removeRequested()
            }
        }
    }
}