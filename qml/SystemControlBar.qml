import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int currentIndex: 0
    signal sectionRequested(int index)
    signal exitRequested()

    implicitHeight: 58
    radius: 16
    color: "#E923252C"
    border.width: 1
    border.color: "#24FFFFFF"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 5

        SectionButton {
            Layout.fillWidth: true
            label: "系统"
            symbol: "◫"
            selected: root.currentIndex === 0
            onClicked: root.sectionRequested(0)
        }

        SectionButton {
            Layout.fillWidth: true
            label: "媒体"
            symbol: "♪"
            selected: root.currentIndex === 1
            onClicked: root.sectionRequested(1)
        }

        SectionButton {
            Layout.fillWidth: true
            label: "通知"
            symbol: "●"
            selected: root.currentIndex === 2
            onClicked: root.sectionRequested(2)
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 28
            color: "#20FFFFFF"
        }

        Rectangle {
            Layout.preferredWidth: 44
            Layout.fillHeight: true
            radius: 11
            color: exitTap.pressed ? "#55FF6565"
                                   : exitHover.hovered ? "#30FF6565" : "transparent"

            Text {
                anchors.centerIn: parent
                text: "⏻"
                color: exitHover.hovered ? "#FFFF9292" : "#A8FFFFFF"
                font.pixelSize: 18
            }

            HoverHandler { id: exitHover }
            TapHandler { id: exitTap; onTapped: root.exitRequested() }
            ToolTip.visible: exitHover.hovered
            ToolTip.text: "退出 Windows Sidebar"
        }
    }

    component SectionButton: Rectangle {
        id: button

        property string label
        property string symbol
        property bool selected: false
        signal clicked()

        Layout.fillHeight: true
        radius: 11
        color: selected ? "#2461A9FF"
                        : buttonHover.hovered ? "#16FFFFFF" : "transparent"

        Row {
            anchors.centerIn: parent
            spacing: 5

            Text {
                text: button.symbol
                color: button.selected ? "#9DCCFF" : "#7FFFFFFF"
                font.pixelSize: 11
            }

            Text {
                text: button.label
                color: button.selected ? "#EFFFFFFF" : "#8FFFFFFF"
                font.pixelSize: 11
                font.weight: button.selected ? Font.DemiBold : Font.Normal
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            width: button.selected ? 18 : 0
            height: 2
            radius: 1
            color: "#71B6FF"
            Behavior on width { NumberAnimation { duration: 140 } }
        }

        HoverHandler { id: buttonHover }
        TapHandler { onTapped: button.clicked() }
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
