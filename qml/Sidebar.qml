import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool expanded: false
    property int currentSection: uiSettings.currentSection
    property bool mediaEnabled: uiSettings.mediaEnabled
    property bool monitorEnabled: uiSettings.monitorEnabled
    property bool notificationsEnabled: uiSettings.notificationsEnabled
    property bool compactMode: uiSettings.compactMode
    property bool transientUiOpen: false
    signal toggleRequested()

    function ensureValidSection() {
        if (root.currentSection >= 4 + pluginRegistry.pageCount)
            uiSettings.currentSection = 0
    }

    Component.onCompleted: ensureValidSection()
    Connections {
        target: pluginRegistry
        function onPagesChanged() { root.ensureValidSection() }
    }

    HoverHandler { id: sidebarHover }

    Timer {
        interval: 900
        running: uiSettings.autoHide && root.expanded && !sidebarHover.hovered && !root.transientUiOpen
        onTriggered: uiSettings.expanded = false
    }

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
        hoverEnabled: true
        onEntered: if (uiSettings.autoHide) uiSettings.expanded = true
        onClicked: root.toggleRequested()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compactMode ? 16 : 24
        anchors.rightMargin: root.compactMode ? 16 : 24
        anchors.topMargin: root.compactMode ? 10 : 18
        anchors.bottomMargin: root.compactMode ? 10 : 16
        spacing: root.compactMode ? 8 : 14
        visible: root.expanded

        Timebar {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactMode ? 52 : 64
            onPopupVisibilityChanged: visible => root.transientUiOpen = visible
        }

        StackLayout {
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
                        Layout.preferredHeight: visible ? 152 : 0
                        moduleEnabled: root.mediaEnabled
                    }

                    NotificationArea {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.notificationsEnabled
                        model: notificationModel
                    }
                }
            }

            FocusPage {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            ClipboardPage {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Repeater {
                model: pluginRegistry.pages
                Loader {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    source: modelData.source

                    Rectangle {
                        anchors.fill: parent
                        visible: parent.status === Loader.Error
                        radius: 16
                        color: themeManager.surface
                        border.width: 1
                        border.color: themeManager.danger
                        Column {
                            anchors.centerIn: parent; spacing: 6
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "插件页面加载失败"; color: themeManager.danger; font.pixelSize: 12; font.weight: Font.DemiBold }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.pluginName; color: themeManager.textMuted; font.pixelSize: 9 }
                        }
                    }
                }
            }
        }

        SystemControlBar {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactMode ? 48 : 54
            currentIndex: root.currentSection
            mediaEnabled: root.mediaEnabled
            monitorEnabled: root.monitorEnabled
            notificationsEnabled: root.notificationsEnabled
            pluginPages: pluginRegistry.pages
            onSectionRequested: index => uiSettings.currentSection = index
            onMediaEnabledRequested: enabled => uiSettings.mediaEnabled = enabled
            onMonitorEnabledRequested: enabled => uiSettings.monitorEnabled = enabled
            onNotificationsEnabledRequested: enabled => uiSettings.notificationsEnabled = enabled
            onPopupVisibilityChanged: visible => root.transientUiOpen = visible
            onExitRequested: Qt.quit()
        }
    }

    Rectangle {
        anchors.left: displayManager.dockSide === 1 ? parent.left : undefined
        anchors.right: displayManager.dockSide === 0 ? parent.right : undefined
        anchors.verticalCenter: parent.verticalCenter
        width: root.expanded ? 12 : 6
        height: root.expanded ? 40 : 52
        radius: width / 2
        color: toggleHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
        Text { anchors.centerIn: parent; visible: root.expanded; text: displayManager.dockSide === 1 ? "‹" : "›"; color: themeManager.textSecondary; font.pixelSize: 14 }
        HoverHandler { id: toggleHover }
        TapHandler { onTapped: root.toggleRequested() }
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
