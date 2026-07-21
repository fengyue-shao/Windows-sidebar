import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool expanded: false
    property int currentSection: 0
    signal toggleRequested()

    Rectangle {
        anchors.fill: parent
        color: "#F21A1B20"
        border.width: root.expanded ? 1 : 0
        border.color: "#30FFFFFF"

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: "#3AFFFFFF"
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: !root.expanded
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggleRequested()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 18
        anchors.topMargin: 16
        anchors.bottomMargin: 14
        spacing: 13
        visible: root.expanded

        Timebar {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
        }

        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentSection

            SystemMonitorPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: "媒体控制"
                            color: "#EFFFFFFF"
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }

                        Text {
                            visible: mediaMonitor.title.length > 0
                            text: mediaMonitor.playing ? "正在播放" : "已暂停"
                            color: mediaMonitor.playing ? "#75BFFFA1" : "#65FFFFFF"
                            font.pixelSize: 10
                        }
                    }

                    MediaBar {
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 138 : 0
                    }

                    Item { Layout.fillHeight: true }
                }

                Column {
                    anchors.centerIn: parent
                    visible: mediaMonitor.title.length === 0
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "♪"
                        color: "#4261A9FF"
                        font.pixelSize: 36
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "暂无媒体会话"
                        color: "#70FFFFFF"
                        font.pixelSize: 13
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "开始播放音乐或视频后会显示在这里"
                        color: "#45FFFFFF"
                        font.pixelSize: 10
                    }
                }
            }

            NotificationArea {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: notificationModel
            }
        }

        SystemControlBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            currentIndex: root.currentSection
            onSectionRequested: index => root.currentSection = index
            onExitRequested: Qt.quit()
        }
    }

    Rectangle {
        id: toggleButton
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: root.expanded ? 22 : 8
        height: 72
        radius: 11
        color: toggleHover.hovered ? "#5461A9FF" : "#344A4D58"

        Text {
            anchors.centerIn: parent
            visible: root.expanded
            text: "›"
            color: "#DFFFFFFF"
            font.pixelSize: 22
        }

        HoverHandler { id: toggleHover }
        TapHandler { onTapped: root.toggleRequested() }
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
