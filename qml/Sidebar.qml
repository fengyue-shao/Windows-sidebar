import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool expanded: false
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
        anchors.topMargin: 18
        anchors.bottomMargin: 18
        spacing: 14
        visible: root.expanded
        opacity: root.expanded ? 1 : 0

        Timebar {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
        }

        Text {
            text: "系统概览"
            color: "#A8FFFFFF"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            spacing: 10

            WidgetCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "CPU"
                value: cpuMonitor.usage.toFixed(0) + "%"
                progress: cpuMonitor.usage / 100
                accent: "#61A9FF"
            }

            WidgetCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "内存"
                value: memoryMonitor.usage.toFixed(0) + "%"
                progress: memoryMonitor.usage / 100
                accent: "#9B8CFF"
            }

            WidgetCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: "GPU"
                value: gpuMonitor.usage.toFixed(0) + "%"
                progress: gpuMonitor.usage / 100
                accent: "#F19A68"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: 14
            color: "#12FFFFFF"
            border.width: 1
            border.color: "#18FFFFFF"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text { text: "下载"; color: "#80FFFFFF"; font.pixelSize: 11 }
                    Text { text: networkMonitor.downloadSpeedText; color: "#F2FFFFFF"; font.pixelSize: 15; font.weight: Font.DemiBold }
                }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 30; color: "#20FFFFFF" }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text { text: "上传"; color: "#80FFFFFF"; font.pixelSize: 11 }
                    Text { text: networkMonitor.uploadSpeedText; color: "#F2FFFFFF"; font.pixelSize: 15; font.weight: Font.DemiBold }
                }
            }
        }

        DiskPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            model: diskMonitor
        }

        MediaBar {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 138 : 0
        }

        NotificationArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: notificationModel
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
            rotation: root.expanded ? 0 : 180
            color: "#DFFFFFFF"
            font.pixelSize: 22
        }

        HoverHandler { id: toggleHover }
        TapHandler { onTapped: root.toggleRequested() }

        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
