import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property date now: new Date()

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

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: themeManager.border
    }
}
