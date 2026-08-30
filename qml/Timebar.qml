import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property date now: new Date()
    property date displayedMonth: new Date(now.getFullYear(), now.getMonth(), 1)
    signal popupVisibilityChanged(bool visible)

    function sameDay(left, right) {
        return left.getFullYear() === right.getFullYear()
            && left.getMonth() === right.getMonth()
            && left.getDate() === right.getDate()
    }

    function monthTitle() {
        return displayedMonth.getFullYear() + "年 " + (displayedMonth.getMonth() + 1) + "月"
    }

    function gridDate(index) {
        const first = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth(), 1)
        const mondayOffset = (first.getDay() + 6) % 7
        return new Date(first.getFullYear(), first.getMonth(), 1 - mondayOffset + index)
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 11

        Text {
            text: Qt.formatTime(root.now, "HH:mm")
            color: themeManager.textPrimary
            font.pixelSize: 29
            font.weight: Font.Medium
            font.letterSpacing: -0.8
        }

        Rectangle {
            Layout.preferredWidth: secondsText.implicitWidth + 12
            Layout.preferredHeight: 23
            radius: 8
            color: themeManager.accentSoft

            Text {
                id: secondsText
                anchors.centerIn: parent
                text: Qt.formatTime(root.now, "ss")
                color: themeManager.accent
                font.pixelSize: 11
                font.family: "Consolas"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.alignment: Qt.AlignRight
                text: root.now.toLocaleDateString(Qt.locale(), "dddd")
                color: themeManager.textSecondary
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignRight
                text: root.now.toLocaleDateString(Qt.locale(), "M月d日")
                color: themeManager.textMuted
                font.pixelSize: 11
            }
        }
    }

    HoverHandler { id: timeHover }
    TapHandler { onTapped: calendarPopup.open() }
    ToolTip.visible: timeHover.hovered && !calendarPopup.visible
    ToolTip.text: "打开月历"

    Popup {
        id: calendarPopup
        x: 0
        y: root.height + 8
        width: root.width
        height: 360
        padding: 14
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onVisibleChanged: root.popupVisibilityChanged(visible)
        onOpened: root.displayedMonth = new Date(root.now.getFullYear(), root.now.getMonth(), 1)
        background: Rectangle {
            radius: 22
            color: themeManager.background
            border.width: 1
            border.color: themeManager.borderStrong
        }

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: root.monthTitle(); color: themeManager.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
                CalendarButton { symbol: "‹"; onClicked: root.displayedMonth = new Date(root.displayedMonth.getFullYear(), root.displayedMonth.getMonth() - 1, 1) }
                CalendarButton { symbol: "今"; onClicked: root.displayedMonth = new Date(root.now.getFullYear(), root.now.getMonth(), 1) }
                CalendarButton { symbol: "›"; onClicked: root.displayedMonth = new Date(root.displayedMonth.getFullYear(), root.displayedMonth.getMonth() + 1, 1) }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 2
                Repeater {
                    model: ["一", "二", "三", "四", "五", "六", "日"]
                    Text {
                        required property string modelData
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: themeManager.textFaint
                        font.pixelSize: 9
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 7
                rows: 6
                rowSpacing: 3
                columnSpacing: 3

                Repeater {
                    model: 42
                    Rectangle {
                        required property int index
                        readonly property date cellDate: root.gridDate(index)
                        readonly property bool today: root.sameDay(cellDate, root.now)
                        readonly property bool inMonth: cellDate.getMonth() === root.displayedMonth.getMonth()
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 10
                        color: today ? themeManager.accent : dayHover.hovered ? themeManager.surfaceHover : themeManager.overlayStart
                        border.width: today ? 0 : 1
                        border.color: inMonth ? themeManager.border : themeManager.overlayStart
                        Text {
                            anchors.centerIn: parent
                            text: parent.cellDate.getDate()
                            color: parent.today ? "#FFFFFF" : parent.inMonth ? themeManager.textPrimary : themeManager.textFaint
                            font.pixelSize: 10
                            font.weight: parent.today ? Font.Bold : Font.Normal
                        }
                        HoverHandler { id: dayHover }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.now.toLocaleDateString(Qt.locale(), "yyyy年M月d日 dddd")
                horizontalAlignment: Text.AlignHCenter
                color: themeManager.textMuted
                font.pixelSize: 10
            }
        }
    }

    component CalendarButton: Rectangle {
        id: button
        property string symbol
        signal clicked()
        Layout.preferredWidth: 32
        Layout.preferredHeight: 30
        radius: 10
        color: buttonHover.hovered ? themeManager.surfaceHover : themeManager.surface
        border.width: 1
        border.color: themeManager.border
        Text { anchors.centerIn: parent; text: button.symbol; color: themeManager.textSecondary; font.pixelSize: 12 }
        HoverHandler { id: buttonHover }
        TapHandler { onTapped: button.clicked() }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: themeManager.border
    }
}
