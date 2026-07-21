import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var model

    implicitHeight: 42 + Math.min(5, diskMonitor.count) * 42
    radius: 14
    color: themeManager.surface
    border.width: 1
    border.color: themeManager.border
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
                color: themeManager.textSecondary
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Text {
                text: diskMonitor.count + " 个分区"
                color: themeManager.textFaint
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
                        color: themeManager.textPrimary
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
                                color: themeManager.textMuted
                                font.pixelSize: 9
                            }

                            Text {
                                text: model.usage.toFixed(0) + "%"
                                color: model.usage >= 90 ? themeManager.danger : themeManager.textSecondary
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 3
                            radius: 2
                            color: themeManager.track

                            Rectangle {
                                width: parent.width * Math.min(1, model.usage / 100)
                                height: parent.height
                                radius: parent.radius
                                color: model.usage >= 90 ? themeManager.danger : themeManager.success
                                Behavior on width { NumberAnimation { duration: 300 } }
                            }
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: diskMonitor.count > 5 ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
        }
    }
}
