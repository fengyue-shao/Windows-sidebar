import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Item {
    id: root
    property bool expanded: false
    property int currentSection: uiSettings.currentSection
    property bool calendarOpen: false
    property bool settingsOpen: false
    readonly property bool transientUiOpen: calendarOpen || settingsOpen
    readonly property bool interacting: pointerGuard.pressed
    signal expandRequested()
    signal collapseRequested()
    signal hideRequested()
    function closeTransientUi() { timebar.closeCalendar(); navigation.closeSettings() }
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
        let regions = [rectFor(topbar, 18)]
        if (!expanded) return regions
        regions.push(rectFor(navigation, 16))
        if (currentSection === 0) regions = regions.concat(monitor.inputRegions(root))
        else regions.push(rectFor(pageSurface, 18))
        return regions
    }
    Component.onCompleted: ensureValidSection()
    Connections { target: pluginRegistry; function onPagesChanged() { root.ensureValidSection() } }
    TapHandler { id: pointerGuard; gesturePolicy: TapHandler.DragThreshold; acceptedButtons: Qt.AllButtons }
    Rectangle {
        id: topbar
        width: parent.width; height: 64; radius: 18
        color: themeManager.cardSurface; border.width: 1; border.color: themeManager.borderStrong
        RowLayout {
            anchors.fill: parent; anchors.margins: 10; spacing: 12
            Text { visible: root.width > 600; text: "Windows Sidebar"; color: themeManager.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
            Timebar {
                id: timebar
                Layout.preferredWidth: root.width > 600 ? 174 : 145
                Layout.fillHeight: true
                onPopupVisibilityChanged: visible => {
                    root.calendarOpen = visible
                    if (visible && root.Window.window) root.Window.window.requestActivate()
                }
            }
            Text {
                Layout.fillWidth: true
                text: (systemTrayManager.online ? "联网" : "离线") + " · "
                    + (systemTrayManager.muted ? "静音" : "音量 " + systemTrayManager.volume + "%")
                    + (systemTrayManager.batteryPresent ? " · 电量 " + systemTrayManager.batteryPercent + "%" : "")
                elide: Text.ElideRight; color: themeManager.textSecondary; font.pixelSize: 11
            }
            BarAction { label: root.expanded ? "收起 ⌃" : "展开 ⌄"; onClicked: root.expanded ? root.collapseRequested() : root.expandRequested() }
            BarAction { label: "隐藏"; onClicked: root.hideRequested() }
        }
    }
    SystemControlBar {
        id: navigation
        x: 0; y: 76; width: parent.width; height: 54; visible: root.expanded
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
        x: 0; y: 142; width: parent.width; height: Math.max(0, parent.height - y - 8)
        visible: root.expanded; clip: true
        DropdownMonitor { id: monitor; anchors.fill: parent; visible: root.currentSection === 0 }
        Rectangle {
            id: pageSurface
            anchors.fill: parent; visible: root.currentSection !== 0; radius: 18
            color: themeManager.cardSurface; border.width: 1; border.color: themeManager.border
            StackLayout {
                anchors.fill: parent; anchors.margins: 18; currentIndex: root.currentSection - 1
                ColumnLayout {
                    spacing: 12
                    Text { text: "通知与媒体"; color: themeManager.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
                    MediaBar { Layout.fillWidth: true; Layout.preferredHeight: visible ? 152 : 0; moduleEnabled: uiSettings.mediaEnabled }
                    NotificationArea { Layout.fillWidth: true; Layout.fillHeight: true; visible: uiSettings.notificationsEnabled; model: notificationModel }
                }
                FocusPage { Layout.fillWidth: true; Layout.fillHeight: true }
                ClipboardPage { Layout.fillWidth: true; Layout.fillHeight: true }
                ToolboxPage { Layout.fillWidth: true; Layout.fillHeight: true }
                Repeater {
                    model: pluginRegistry.pages
                    Loader {
                        id: pluginLoader
                        required property int index
                        required property var modelData
                        property bool visited: false
                        readonly property bool selected: root.currentSection === 5 + index
                        onSelectedChanged: if (selected) visited = true
                        Component.onCompleted: if (selected) visited = true
                        Layout.fillWidth: true; Layout.fillHeight: true
                        source: modelData.source; active: visited
                        Column {
                            anchors.centerIn: parent; spacing: 8; visible: pluginLoader.status === Loader.Error
                            Text { text: "插件加载失败：" + modelData.pluginName; color: themeManager.danger }
                            Button { text: "重新加载"; onClicked: { pluginLoader.visited = false; pluginLoader.visited = true } }
                        }
                    }
                }
            }
        }
    }
    component BarAction: Basic.Button {
        property string label
        text: label; implicitWidth: 64; implicitHeight: 38
        contentItem: Text { text: parent.text; color: themeManager.accent; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
        background: Rectangle { radius: 12; color: parent.hovered || parent.down ? themeManager.accentSoft : "transparent" }
    }
}
