import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string label: "指标"
    property string value: "0%"
    property real progress: 0
    property color accent: "#61A9FF"

    radius: 12
    color: hover.hovered ? "#19FFFFFF" : "#0EFFFFFF"
    border.width: 1
    border.color: "#16FFFFFF"

    HoverHandler { id: hover }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 9

        Rectangle {
            Layout.preferredWidth: 5
            Layout.preferredHeight: 28
            radius: 3
            color: "#20FFFFFF"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height * Math.max(0.06, Math.min(1, root.progress))
                radius: parent.radius
                color: root.accent
                Behavior on height { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                text: root.label
                color: "#72FFFFFF"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Text {
                text: root.value
                color: "#EFFFFFFF"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
        }
    }

    Behavior on color { ColorAnimation { duration: 100 } }
}
