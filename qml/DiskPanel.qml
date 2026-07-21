import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var model

    implicitHeight: 42 + Math.min(3, diskMonitor.count) * 42
    radius: 14
    color: "#0EFFFFFF"
    border.width: 1
    border.color: "#16FFFFFF"
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 13
        anchors.rightMargin: 10
        anchors.topMargin: 9
        anchors.bottomMargin: 9
        spacing: 5

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: "磁盘空间"
                color: "#A8FFFFFF"
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Text {
                text: diskMonitor.count + " 个分区"
                color: "#58FFFFFF"
                font.pixelSize: 9
            }
        }

        ListView {
            id: partitionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.model
            spacing: 5
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                width: partitionList.width
                height: 37

                RowLayout {
                    anchors.fill: parent
                    spacing: 9

                    Text {
                        Layout.preferredWidth: 28
                        text: model.rootPath.substring(0, 2)
                        color: "#EFFFFFFF"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: model.usedText + " / " + model.totalText
                                color: "#86FFFFFF"
                                font.pixelSize: 9
                            }

                            Text {
                                text: model.usage.toFixed(0) + "%"
                                color: model.usage >= 90 ? "#FFFF8989" : "#A8FFFFFF"
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 3
                            radius: 2
                            color: "#18FFFFFF"

                            Rectangle {
                                width: parent.width * Math.min(1, model.usage / 100)
                                height: parent.height
                                radius: parent.radius
                                color: model.usage >= 90 ? "#FF6F6F" : "#62C4A4"
                                Behavior on width { NumberAnimation { duration: 300 } }
                            }
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: diskMonitor.count > 3 ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
        }
    }
}
