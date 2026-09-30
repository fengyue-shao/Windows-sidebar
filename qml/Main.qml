import QtQuick
import QtQuick.Window

Window {
    id: root
    title: "Windows Sidebar · 0.8.1 顶部下拉预览"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        | (shellState === 2 || sidebar.calendarOpen ? 0 : Qt.WindowDoesNotAcceptFocus)
    color: "transparent"
    // Startup never restores expanded cards. Legacy expanded stays available to plugins.
    property int shellState: 0 // Hidden=0, Bar=1, Cards=2
    readonly property var workArea: displayManager.availableGeometry
    readonly property int expandedHeight: Math.max(160, Math.min(780, workArea.height - 16))
    readonly property bool editing: active && activeFocusItem !== null
        && (activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit)
    width: Math.max(1, Math.min(Math.round(uiSettings.panelWidth * 1.8), workArea.width - 24))
    height: shellState === 2 ? expandedHeight : sidebar.calendarOpen ? Math.min(460, expandedHeight) : 72
    x: workArea.x + Math.round((workArea.width - width) / 2)
    y: workArea.y + 6
    opacity: uiSettings.panelOpacity
    visible: shellState !== 0
    function showBar() { if (shellState === 0) shellState = 1; syncRegions() }
    function hidePanel() { sidebar.closeTransientUi(); dropdownController.blockEdgeUntilLeave(); shellState = 0 }
    function toggleBar() {
        if (shellState === 0) showBar()
        else hidePanel()
    }
    function showCards() { sidebar.closeTransientUi(); shellState = 2; requestActivate(); syncRegions() }
    function collapseCards() { sidebar.closeTransientUi(); sidebar.forceActiveFocus(); shellState = 1; syncRegions() }
    function syncRegions() {
        dropdownController.configureEdge(x, displayManager.selectedY, width)
        if (visible) dropdownController.setRegions(sidebar.inputRegions())
    }
    onShellStateChanged: Qt.callLater(syncRegions)
    onWidthChanged: Qt.callLater(syncRegions)
    onHeightChanged: Qt.callLater(syncRegions)
    onXChanged: Qt.callLater(syncRegions)
    onYChanged: Qt.callLater(syncRegions)
    Component.onCompleted: { dropdownController.attach(root); syncRegions(); if (previewCards) showCards() }
    Connections {
        target: dropdownController
        function onEdgeRequested() { root.showBar() }
        function onHotkeyRequested() { root.toggleBar() }
    }
    Timer { interval: 80; running: root.visible; repeat: true; onTriggered: root.syncRegions() }
    Timer {
        interval: root.shellState === 1 ? 4000 : 1200
        running: root.visible && (uiSettings.autoHide || root.shellState === 1)
            && !dropdownController.pointerInside && !sidebar.transientUiOpen && !root.editing && !sidebar.interacting
        onTriggered: root.hidePanel()
    }
    Sidebar {
        id: sidebar
        anchors.fill: parent
        expanded: root.shellState === 2
        onExpandRequested: root.showCards()
        onCollapseRequested: root.collapseCards()
        onHideRequested: root.hidePanel()
    }
    Shortcut { sequence: "Ctrl+Space"; onActivated: root.toggleBar() }
    Shortcut { sequence: "Escape"; enabled: !sidebar.transientUiOpen; onActivated: root.shellState === 2 ? root.collapseCards() : root.hidePanel() }
    Shortcut { sequence: "Ctrl+1"; onActivated: { uiSettings.currentSection = 0; root.showCards() } }
    Shortcut { sequence: "Ctrl+2"; onActivated: { uiSettings.currentSection = 1; root.showCards() } }
    Shortcut { sequence: "Ctrl+3"; onActivated: { uiSettings.currentSection = 2; root.showCards() } }
    Shortcut { sequence: "Ctrl+4"; onActivated: { uiSettings.currentSection = 3; root.showCards() } }
    Shortcut { sequence: "Ctrl+5"; onActivated: { uiSettings.currentSection = 4; root.showCards() } }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
    Timer {
        property int step: 0
        interval: 180; running: uiSelfTest; repeat: true
        onTriggered: {
            if (step === 0) { root.hidePanel(); root.showBar() }
            else if (step === 1) {
                if (root.shellState !== 1 || root.height > 80) { console.error("UI_SELF_TEST Bar failed"); Qt.exit(2); return }
                root.showCards()
            } else if (step === 2) {
                if (root.shellState !== 2 || root.height < 160) { console.error("UI_SELF_TEST Cards failed"); Qt.exit(2); return }
                sidebar.currentSection = 0
                root.syncRegions()
                if (!dropdownController.regionContains(Math.round(root.width/2), 32)
                    || dropdownController.regionContains(Math.round(root.width/2), 70)
                    || (root.width >= 620 && dropdownController.regionContains(Math.round(root.width/2), 180))) {
                    console.error("UI_SELF_TEST input regions failed"); Qt.exit(3); return
                }
                root.collapseCards()
            } else if (step === 3) { if (root.shellState !== 1) { Qt.exit(2); return }; root.hidePanel() }
            else if (step === 4) {
                if (root.visible) { Qt.exit(2); return }
                root.showCards()
            } else if (step >= 5 && step < 10 + pluginRegistry.pageCount) {
                sidebar.currentSection = step - 5
            } else {
                console.info("UI_SELF_TEST PASS Hidden/Bar/Cards + geometry + input regions + pages"); Qt.quit()
            }
            step++
        }
    }
}
