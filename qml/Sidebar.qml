import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Item {
    id: root
    property bool expanded: false
    property bool islandOnly: false
    readonly property alias islandItem: island
    readonly property bool compactTopbar: width < 900
    readonly property int barHeight: topbar.height + 8
    readonly property int bodyTop: topbar.height + 78
    property int currentSection: uiSettings.currentSection
    property bool calendarOpen: false
    property bool settingsOpen: false
    readonly property bool transientUiOpen: calendarOpen || settingsOpen || notifications.popupOpen
    readonly property bool interacting: pointerGuard.pressed
    signal expandRequested()
    signal collapseRequested()
    signal hideRequested()
    function toggleCards() { expanded ? collapseRequested() : expandRequested() }
    function openSettings() { navigation.openSettings() }
    onSettingsOpenChanged: if (settingsOpen && root.Window.window) Qt.callLater(root.Window.window.requestActivate)
    function closeTransientUi() { timebar.closeCalendar(); navigation.closeSettings(); notifications.closePopups() }
    function ensureValidSection() {
        if (currentSection < 0 || currentSection >= 5 + pluginRegistry.pageCount) uiSettings.currentSection = 4
    }
    function rectFor(item, radius) {
        const p = item.mapToItem(root, 0, 0)
        return { x: p.x, y: p.y, width: item.width, height: item.height, radius: radius }
    }
    function inputRegions() {
        // Popups temporarily need outside-press handling; normal gaps stay click-through.
        if (transientUiOpen) return [{x: 0, y: 0, width: width, height: height}]
        let regions = [rectFor(island, island.radius)]
        if (!islandOnly) regions.push(rectFor(topbar, 18))
        if (!expanded) return regions
        regions.push(rectFor(navigation, 16))
        if (currentSection === 0) regions = regions.concat(monitor.inputRegions(root))
        else if (currentSection === 1) regions = regions.concat(notifications.inputRegions(root))
        else if (currentSection === 2) regions = regions.concat(focus.inputRegions(root))
        else if (currentSection === 3) regions = regions.concat(clipboard.inputRegions(root))
        else if (currentSection === 4) regions = regions.concat(toolbox.inputRegions(root))
        else regions.push(rectFor(legacySurface, 18))
        return regions
    }
    Component.onCompleted: ensureValidSection()
    Connections { target: pluginRegistry; function onPagesChanged() { root.ensureValidSection() } }
    TapHandler { id: pointerGuard; gesturePolicy: TapHandler.DragThreshold; acceptedButtons: Qt.AllButtons }
    Rectangle {
        id: topbar
        visible: !root.islandOnly
        opacity: uiSettings.panelOpacity
        width: parent.width; height: root.compactTopbar ? 136 : 80; radius: 18
        color: themeManager.cardSurface; border.width: 1; border.color: themeManager.borderStrong
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            y: root.compactTopbar ? 64 : 8; width: Math.min(420, root.width - 24); height: 64
            TapHandler { onDoubleTapped: root.toggleCards() }
        }
        RowLayout {
            anchors.left: parent.left; anchors.leftMargin: 12
            y: 8; height: root.compactTopbar ? 44 : 64
            width: root.compactTopbar ? 174 : (root.width - 420) / 2 - 24
            spacing: 12
            Text { visible: !root.compactTopbar && parent.width > 340; text: "Windows Sidebar"; color: themeManager.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
            Timebar {
                id: timebar
                Layout.preferredWidth: root.width > 600 ? 174 : 145
                Layout.fillHeight: true
                onPopupVisibilityChanged: visible => {
                    root.calendarOpen = visible
                    if (visible && root.Window.window) root.Window.window.requestActivate()
                }
            }
            Item { Layout.fillWidth: true }
        }
        RowLayout {
            anchors.right: parent.right; anchors.rightMargin: 12
            y: 8; height: root.compactTopbar ? 44 : 64
            width: root.compactTopbar ? root.width - 210 : (root.width - 420) / 2 - 24
            spacing: 12
            Item { Layout.fillWidth: true }
            Status { iconName: systemTrayManager.online ? "wifi_1" : "wifi_off"; label: systemTrayManager.online ? "联网" : "离线" }
            Status { iconName: systemTrayManager.muted ? "speaker_mute" : "speaker_2"; label: systemTrayManager.muted ? "静音" : systemTrayManager.volume + "%" }
            Status { visible: systemTrayManager.batteryPresent; iconName: "battery_10"; label: systemTrayManager.batteryPercent + "%" }
            Basic.Button {
                implicitWidth: 40; implicitHeight: 40
                onClicked: { timebar.closeCalendar(); navigation.openSettings() }
                contentItem: FluentIcon { name: "settings"; tint: themeManager.accent }
                background: Rectangle { radius: 12; color: parent.hovered ? themeManager.accentSoft : "transparent" }
                ToolTip.visible: hovered; ToolTip.text: "设置"
            }
        }
    }
    TopIslandHost {
        id: island
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.islandOnly ? 0 : (root.compactTopbar ? 64 : 8) + (64 - height) / 2
        availableWidth: Math.max(64, root.width - 24)
        onCardsToggleRequested: root.toggleCards()
    }
    SystemControlBar {
        id: navigation
        x: 0; y: topbar.height + 12; width: parent.width; height: 54; visible: root.expanded
        opacity: uiSettings.panelOpacity
        currentIndex: root.currentSection
        mediaEnabled: uiSettings.mediaEnabled; monitorEnabled: uiSettings.monitorEnabled; notificationsEnabled: uiSettings.notificationsEnabled
        pluginPages: pluginRegistry.pages
        onSectionRequested: index => uiSettings.currentSection = index
        onMediaEnabledRequested: enabled => uiSettings.mediaEnabled = enabled
        onMonitorEnabledRequested: enabled => uiSettings.monitorEnabled = enabled
        onNotificationsEnabledRequested: enabled => uiSettings.notificationsEnabled = enabled
        onPopupVisibilityChanged: visible => root.settingsOpen = visible
        onExitRequested: Qt.quit()
    }
    Item {
        id: body
        x: 0; y: root.bodyTop; width: parent.width; height: Math.max(0, parent.height - y - 8)
        opacity: uiSettings.panelOpacity
        visible: root.expanded; clip: true
        DropdownMonitor { id: monitor; anchors.fill: parent; visible: root.currentSection === 0 }
        Rectangle {
            id: legacySurface
            anchors.fill: parent; visible: root.currentSection >= 5; radius: 18
            color: themeManager.cardSurface; border.width: 1; border.color: themeManager.border
        }
        Item {
            anchors.fill: parent; visible: root.currentSection !== 0
            StackLayout {
                anchors.fill: parent; currentIndex: root.currentSection - 1
                NotificationPage { id: notifications; Layout.fillWidth: true; Layout.fillHeight: true }
                FocusPage { id: focus; Layout.fillWidth: true; Layout.fillHeight: true }
                ClipboardPage { id: clipboard; Layout.fillWidth: true; Layout.fillHeight: true }
                ToolboxPage { id: toolbox; Layout.fillWidth: true; Layout.fillHeight: true }
                Repeater {
                    model: pluginRegistry.pages
                    Item {
                        id: pluginHost
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true; Layout.fillHeight: true
                    Loader {
                        id: pluginLoader
                        anchors.fill: parent; anchors.margins: 18
                        property bool visited: false
                        readonly property bool selected: root.currentSection === 5 + pluginHost.index
                        onSelectedChanged: if (selected) visited = true
                        Component.onCompleted: if (selected) visited = true
                        source: pluginHost.modelData.source; active: visited
                        Column {
                            anchors.centerIn: parent; spacing: 8; visible: pluginLoader.status === Loader.Error
                            Text { text: "插件加载失败：" + pluginHost.modelData.pluginName; color: themeManager.danger }
                            Button { text: "重新加载"; onClicked: { pluginLoader.visited = false; pluginLoader.visited = true } }
                        }
                    }
                    }
                }
            }
        }
    }
    component Status: RowLayout {
        property string iconName
        property string label
        spacing: 6
        FluentIcon { name: parent.iconName; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
        Text { text: parent.label; color: themeManager.textSecondary; font.pixelSize: 11 }
    }
}
