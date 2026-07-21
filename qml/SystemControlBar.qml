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
    color: themeManager.surfaceStrong
    border.width: 1
    border.color: themeManager.borderStrong

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
            color: themeManager.track
        }

        Rectangle {
            Layout.preferredWidth: 44
            Layout.fillHeight: true
            radius: 11
            color: settingsTap.pressed ? themeManager.accentSoft
                                       : settingsHover.hovered ? themeManager.surfaceHover : themeManager.overlayStart

            Text {
                anchors.centerIn: parent
                text: "⚙"
                color: settingsHover.hovered ? themeManager.textPrimary : themeManager.textSecondary
                font.pixelSize: 17
            }

            HoverHandler { id: settingsHover }
            TapHandler { id: settingsTap; onTapped: settingsMenu.open() }
            ToolTip.visible: settingsHover.hovered
            ToolTip.text: "设置"
        }
    }

    Menu {
        id: settingsMenu
        x: root.width - width
        y: -height - 6

        MenuItem {
            text: "跟随系统"
            checkable: true
            checked: themeManager.mode === 0
            onTriggered: themeManager.setMode(0)
        }
        MenuItem {
            text: "白天模式"
            checkable: true
            checked: themeManager.mode === 1
            onTriggered: themeManager.setMode(1)
        }
        MenuItem {
            text: "黑夜模式"
            checkable: true
            checked: themeManager.mode === 2
            onTriggered: themeManager.setMode(2)
        }

        MenuSeparator { }

        MenuItem {
            text: "退出 Windows Sidebar"
            onTriggered: root.exitRequested()
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
        color: selected ? themeManager.accentSoft
                        : buttonHover.hovered ? themeManager.surfaceHover : themeManager.overlayStart

        Row {
            anchors.centerIn: parent
            spacing: 5

            Text {
                text: button.symbol
                color: button.selected ? themeManager.accent : themeManager.textMuted
                font.pixelSize: 11
            }

            Text {
                text: button.label
                color: button.selected ? themeManager.textPrimary : themeManager.textMuted
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
            color: themeManager.accent
            Behavior on width { NumberAnimation { duration: 140 } }
        }

        HoverHandler { id: buttonHover }
        TapHandler { onTapped: button.clicked() }
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
