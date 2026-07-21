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
        color: themeManager.background
        border.width: root.expanded ? 1 : 0
        border.color: themeManager.borderStrong
        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 1; color: themeManager.borderStrong }
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

        Timebar { Layout.fillWidth: true; Layout.preferredHeight: 58 }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentSection

            SystemMonitorPanel { Layout.fillWidth: true; Layout.fillHeight: true }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "通知与媒体"
                            color: themeManager.textPrimary
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Text {
                            visible: mediaMonitor.title.length > 0
                            text: mediaMonitor.playing ? "正在播放" : "已暂停"
                            color: mediaMonitor.playing ? themeManager.success : themeManager.textMuted
                            font.pixelSize: 10
                        }
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
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: root.expanded ? 22 : 8
        height: 72
        radius: 11
        color: toggleHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
        Text { anchors.centerIn: parent; visible: root.expanded; text: "‹"; color: themeManager.textPrimary; font.pixelSize: 22 }
        HoverHandler { id: toggleHover }
        TapHandler { onTapped: root.toggleRequested() }
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
