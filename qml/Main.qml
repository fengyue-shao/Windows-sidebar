import QtQuick
import QtQuick.Window

Window {
    id: root
    title: "Windows Sidebar · 0.8.1 顶部下拉预览"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        | (shellState === 2 || sidebar.calendarOpen || sidebar.settingsOpen ? 0 : Qt.WindowDoesNotAcceptFocus)
    color: "transparent"
    // Startup never restores expanded cards. Legacy expanded stays available to plugins.
    property int shellState: 0 // Hidden=0, Bar=1, Cards=2, IslandOnly=3
    property bool islandTestWakeEnabled: false
    readonly property var workArea: displayManager.availableGeometry
    readonly property int expandedHeight: Math.max(160, Math.min(780, workArea.height - 16))
    readonly property bool editing: active && activeFocusItem !== null
        && (activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit)
    width: Math.max(1, Math.min(Math.round(uiSettings.panelWidth * 1.8), workArea.width - 24))
    height: shellState === 2 || sidebar.settingsOpen ? expandedHeight : sidebar.calendarOpen ? Math.min(460, expandedHeight) : shellState === 3 ? 72 : sidebar.barHeight
    x: workArea.x + Math.round((workArea.width - width) / 2)
    y: workArea.y + 6
    opacity: 1 // The island stays opaque; other surfaces retain their configured opacity.
    visible: shellState !== 0
    function showBar() { if (shellState === 0 || shellState === 3) shellState = 1; syncRegions() }
    function hidePanel() { sidebar.closeTransientUi(); dropdownController.blockEdgeUntilLeave(); shellState = 0 }
    function toggleBar() {
        if (shellState === 0 || shellState === 3) showBar()
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
    Binding { target: audioInputManager; property: "meteringEnabled"; value: root.visible && ((root.shellState === 2 && sidebar.currentSection === 0) || sidebar.islandItem.inputActive) }
    function spectrumShouldRun(requested, windowVisible, moduleEnabled, playing, mediaCardVisible, mediaIslandVisible) {
        return requested && windowVisible && moduleEnabled && playing && (mediaCardVisible || mediaIslandVisible)
    }
    Binding {
        target: mediaSpectrum; property: "enabled"
        value: root.spectrumShouldRun(mediaSpectrum.requested, root.visible, uiSettings.mediaEnabled, mediaMonitor.playing,
            root.shellState === 2 && sidebar.currentSection === 0, sidebar.islandItem.builtinMedia)
    }
    Component.onCompleted: { dropdownController.attach(root); syncRegions(); if (previewCards) showCards() }
    IslandSources {}
    Connections {
        target: islandBridge
        function onDisplayRequested() {
            if ((!uiSelfTest || root.islandTestWakeEnabled) && root.shellState === 0) {
                root.shellState = 3; root.syncRegions()
            }
        }
        function onActiveStateChanged() {
            if (root.shellState === 3 && !islandBridge.activeState.sourceId) root.hidePanel()
        }
        function onDetailsRequested(sourceId) {
            const route=islandBridge.activeState.route
            if (route==="focus") uiSettings.currentSection=2
            else if (route==="notifications") uiSettings.currentSection=1
            else if (route==="microphone" || route==="media") uiSettings.currentSection=0
            else if (route==="terminal") {
                for (let i=0;i<pluginRegistry.pages.length;++i) {
                    if (pluginRegistry.pages[i].pluginId==="com.windows-sidebar.terminal") { uiSettings.currentSection=5+i; break }
                }
            } else return
            root.showCards()
        }
    }
    Connections {
        target: dropdownController
        function onEdgeRequested() { root.showBar() }
        function onHotkeyRequested() { root.toggleBar() }
    }
    Timer { interval: 80; running: root.visible; repeat: true; onTriggered: root.syncRegions() }
    Timer {
        interval: root.shellState === 1 || root.shellState === 3 ? 4000 : 1200
        running: !previewCards && root.visible && (uiSettings.autoHide || root.shellState === 1 || root.shellState === 3)
            && !islandBridge.eventActive && !dropdownController.pointerInside && !sidebar.transientUiOpen && !root.editing && !sidebar.interacting
        onTriggered: root.hidePanel()
    }
    Sidebar {
        id: sidebar
        anchors.fill: parent
        expanded: root.shellState === 2
        islandOnly: root.shellState === 3
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
    MediaBarSelfTest { id: mediaTests; visible: uiSelfTest; width: 360; height: implicitHeight; y: root.height-height; z: 100 }
    Timer {
        id: layoutCaptureDelay
        interval: 80
        property var callback
        onTriggered: callback()
    }
    Timer {
        property int step: 0
        interval: 180; running: uiSelfTest; repeat: true
        onTriggered: {
            if (step === 0) { root.hidePanel(); root.showBar() }
            else if (step === 1) {
                if (root.shellState !== 1 || root.height !== sidebar.barHeight) { console.error("UI_SELF_TEST Bar failed"); Qt.exit(2); return }
                sidebar.toggleCards()
            } else if (step === 2) {
                if (root.shellState !== 2 || root.height < 160) { console.error("UI_SELF_TEST Cards failed"); Qt.exit(2); return }
                sidebar.currentSection = 0
                root.syncRegions()
                if (!dropdownController.regionContains(Math.round(root.width/2), 32)
                    || dropdownController.regionContains(Math.round(root.width/2), sidebar.barHeight - 2)
                    || (root.width >= 620 && dropdownController.regionContains(Math.round(root.width/2), sidebar.bodyTop + 38))) {
                    console.error("UI_SELF_TEST input regions failed"); Qt.exit(3); return
                }
                root.collapseCards()
            } else if (step === 3) { if (root.shellState !== 1) { Qt.exit(2); return }; root.hidePanel() }
            else if (step === 4) {
                if (root.visible) { Qt.exit(2); return }
                root.showCards()
            } else if (step >= 5 && step < 10 + pluginRegistry.pageCount) {
                // Check the previously laid-out focus page before switching away.
                if (step === 8 && root.width >= 620 && root.height >= 500) {
                    root.syncRegions()
                    if (sidebar.inputRegions().length < 8
                        || dropdownController.regionContains(Math.round(root.width/2), sidebar.bodyTop + 118)) {
                        console.error("UI_SELF_TEST independent focus cards failed"); Qt.exit(4); return
                    }
                }
                sidebar.currentSection = step - 5
            } else if (step === 10 + pluginRegistry.pageCount) {
                root.collapseCards(); sidebar.openSettings()
            } else if (step === 11 + pluginRegistry.pageCount) {
                if (!sidebar.settingsOpen || root.height < 160) { console.error("UI_SELF_TEST minimal bar settings failed"); Qt.exit(4); return }
                sidebar.closeTransientUi(); root.showCards()
            } else {
                running = false
                root.hidePanel(); root.islandTestWakeEnabled = true
                islandBridge.pushEvent("test.ui","wake",{title:"专注已完成",subtitle:"休息一下 · 测试展示",iconName:"timer",route:"focus"},99,10000,15000)
                root.syncRegions()
                if (root.shellState !== 3 || root.height > 80
                    || !dropdownController.regionContains(Math.round(root.width/2), 32)
                    || dropdownController.regionContains(12, 32)) { console.error("UI_SELF_TEST island-only wake / mask failed"); Qt.exit(7); return }
                try {
                    // Acquisition follows visible consumers, not expanded cards alone.
                    const spectrumCases = [
                        [true,true,true,true,false,true,true], // minimal bar / IslandOnly
                        [true,true,true,true,true,false,true], // media card
                        [true,true,true,true,true,true,true], // share one capture
                        [false,true,true,true,true,true,false], // opt out
                        [true,false,true,true,true,true,false], // fully hidden
                        [true,true,false,true,true,true,false], // disabled module
                        [true,true,true,false,true,true,false], // paused
                        [true,true,true,true,false,false,false] // another island has priority
                    ]
                    for (const test of spectrumCases) {
                        if (root.spectrumShouldRun(test[0],test[1],test[2],test[3],test[4],test[5]) !== test[6])
                            throw new Error("Spectrum visibility policy")
                    }
                    console.info("UI_SELF_TEST spectrum collapsed/island/card + hidden/pause/opt-out/preemption policy PASS")
                    const host = sidebar.islandItem
                    const cases = [
                        {},
                        {kind:"input",applicationCount:1},
                        {kind:"input",applicationCount:3},
                        {kind:"input",subtitle:"跨进程或未知来源"},
                        {kind:"media",progress:0.5},
                        {kind:"media"},
                        {kind:"focus",detailText:"18:42",progress:0.4},
                        {kind:"focus",mode:"event",tone:"success"},
                        {kind:"notification",mode:"event"},
                        {kind:"notification",mode:"event",queuedCount:4},
                        {kind:"terminal"},
                        {kind:"terminal",mode:"event",tone:"success"},
                        {kind:"terminal",mode:"event",tone:"danger"},
                        {kind:"focus",mode:"event",tone:"success"}
                    ]
                    for (let i=0; i<cases.length; ++i) {
                        host.stateOverride = i===0 ? {} : Object.assign({sourceId:"test.presentation",mode:"persistent",title:"场景验证",subtitle:"测试状态"},cases[i])
                        const expectedWidth = i===0 ? 64 : cases[i].mode==="event" ? 420 : 344
                        if (host.width !== Math.min(host.availableWidth,expectedWidth)
                            || host.height !== (i===0 ? 24 : cases[i].mode==="event" ? 64 : 52)
                            || host.hasProgress !== (cases[i].progress !== undefined)
                            || host.pendingCount !== (i===9 ? 3 : 0)) throw new Error("Island scenario " + i)
                    }
                    host.stateOverride = null
                    console.info("UI_SELF_TEST island 14 presentation states + timeline absence + queue badge PASS")
                    mediaTests.runTests()
                } catch (error) { sidebar.islandItem.stateOverride=null; console.error("UI_SELF_TEST", error); Qt.exit(4); return }
                console.info("UI_SELF_TEST audio endpoints: input="+audioInputManager.available+" inputMeter="+audioInputManager.inputMeterAvailable+" outputMeter="+audioInputManager.outputMeterAvailable)
                const done = function() { console.info("UI_SELF_TEST PASS Hidden/Bar/Cards/IslandOnly + geometry + input regions + pages"); Qt.quit() }
                const finish = function() {
                    root.showCards()
                    sidebar.currentSection = 0
                    mediaTests.visible = false
                    const cleanup = function() {
                        islandBridge.withdraw("test.ui"); root.islandTestWakeEnabled = false
                        mediaTests.visible = true
                        if (uiCapturePath.length > 0) mediaTests.capture(uiCapturePath,done)
                        else done()
                    }
                    if (uiCapturePath.length > 0) {
                        // Window resize is asynchronous; capture only after the new content geometry settles.
                        layoutCaptureDelay.callback = function() {
                            root.contentItem.grabToImage(function(result) {
                                if (!result.saveToFile(uiCapturePath + ".layout.png")) { Qt.exit(8); return }
                                cleanup()
                            })
                        }
                        layoutCaptureDelay.restart()
                    } else cleanup()
                }
                if (uiCapturePath.length > 0) {
                    sidebar.islandItem.grabToImage(function(result) {
                        if (!result.saveToFile(uiCapturePath + ".island.png")) { Qt.exit(8); return }
                        finish()
                    })
                } else finish()
            }
            step++
        }
    }
}
