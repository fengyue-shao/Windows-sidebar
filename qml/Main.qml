import QtQuick
import QtQuick.Window

Window {
    id: root
    title: "Windows Sidebar · " + Qt.application.version + " · 顶部下拉预览"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        | (shellState === 2 || hasPinnedCards || sidebar.calendarOpen || sidebar.settingsOpen || sidebar.islandItem.controlsOpen ? 0 : Qt.WindowDoesNotAcceptFocus)
    color: "transparent"
    // Startup never restores expanded cards. Legacy expanded stays available to plugins.
    property int shellState: 0 // Hidden=0, Bar=1, Cards=2, IslandOnly=3
    property bool islandTestWakeEnabled: false
    readonly property var workArea: displayManager.availableGeometry
    readonly property int panelWidth: Math.max(1, Math.min(Math.round(uiSettings.panelWidth * 1.8), workArea.width - 24))
    readonly property bool hasPinnedCards: nativeCardPreview && desktopCanvas.item && desktopCanvas.item.hasPinned
    readonly property int expandedHeight: Math.max(160, Math.min(780, workArea.height - 16))
    readonly property bool editing: active && activeFocusItem !== null
        && (activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit)
    width: nativeCardPreview ? workArea.width : panelWidth
    height: nativeCardPreview ? workArea.height : shellState === 2 || sidebar.settingsOpen ? expandedHeight : sidebar.calendarOpen ? Math.min(460, expandedHeight) : Math.max(shellState === 3 ? 72 : sidebar.barHeight, Math.ceil(sidebar.islandItem.y+sidebar.islandItem.height+8))
    x: workArea.x + Math.round((workArea.width - width) / 2)
    y: workArea.y + (nativeCardPreview ? 0 : 6)
    opacity: 1 // The island stays opaque; other surfaces retain their configured opacity.
    visible: shellState !== 0 || hasPinnedCards
    function showBar() { if (shellState === 0 || shellState === 3) shellState = 1; syncRegions() }
    function mayLeaveEditor() {
        if (desktopCanvas.item && desktopCanvas.item.editing) { desktopCanvas.item.interactionHint="请先保存或取消卡片编辑，再切换页面或隐藏顶部栏"; return false }
        return true
    }
    function hidePanel() { if (!mayLeaveEditor()) return; sidebar.closeTransientUi(); dropdownController.blockEdgeUntilLeave(); shellState = 0 }
    function toggleBar() {
        if (shellState === 0 || shellState === 3) showBar()
        else hidePanel()
    }
    function showCards() { sidebar.closeTransientUi(); shellState = 2; requestActivate(); syncRegions() }
    function collapseCards() { if (!mayLeaveEditor()) return; sidebar.closeTransientUi(); sidebar.forceActiveFocus(); shellState = 1; syncRegions() }
    function syncRegions() {
        dropdownController.configureEdge(x+sidebar.x, displayManager.selectedY, sidebar.width)
        let regions=sidebar.visible ? sidebar.inputRegions().map(r=>Object.assign({},r,{x:r.x+sidebar.x,y:r.y+sidebar.y})) : []
        if (desktopCanvas.item) regions=regions.concat(desktopCanvas.item.inputRegions(root.contentItem))
        if (nativeCardPreview && sidebar.visible && sidebar.blockingPopupOpen) regions=[{x:0,y:0,width:root.width,height:root.height}]
        dropdownController.setRegions(regions)
    }
    Binding { target: dropdownController; property: "edgeRevealAllowed"; value: root.shellState === 0 }
    onShellStateChanged: Qt.callLater(syncRegions)
    onWidthChanged: Qt.callLater(syncRegions)
    onHeightChanged: Qt.callLater(syncRegions)
    onXChanged: Qt.callLater(syncRegions)
    onYChanged: Qt.callLater(syncRegions)
    Binding { target: audioInputManager; property: "meteringEnabled"; value: root.visible && (nativeCardPreview ? desktopCanvas.item && desktopCanvas.item.cardVisible("monitor.microphone") || sidebar.visible && sidebar.islandItem.inputActive : (root.shellState === 2 && sidebar.currentSection === 0) || sidebar.islandItem.inputActive) }
    function spectrumShouldRun(requested, windowVisible, moduleEnabled, playing, mediaCardVisible, mediaIslandVisible) {
        return requested && windowVisible && moduleEnabled && playing && (mediaCardVisible || mediaIslandVisible)
    }
    Binding {
        target: mediaSpectrum; property: "enabled"
        value: root.spectrumShouldRun(mediaSpectrum.requested, root.visible, uiSettings.mediaEnabled, mediaMonitor.playing,
            nativeCardPreview ? desktopCanvas.item && desktopCanvas.item.cardVisible("monitor.media") : root.shellState === 2 && sidebar.currentSection === 0, sidebar.visible && sidebar.islandItem.builtinMedia)
    }
    Component.onCompleted: { dropdownController.attach(root); syncRegions(); if (previewCards) showCards() }
    IslandSources {}
    Connections {
        target: sidebar.islandItem
        // Refresh the native input mask on every rendered geometry change,
        // not just the 80 ms fallback poll. Use the actual animated rectangle.
        function onWidthChanged() { root.syncRegions() }
        function onHeightChanged() { root.syncRegions() }
        function onYChanged() { root.syncRegions() }
        function onControlsOpenChanged() { if (sidebar.islandItem.controlsOpen) root.requestActivate(); root.syncRegions() }
        function onActionRequested(sourceId, actionId, targetId) {
            if (sourceId !== islandBridge.activeState.sourceId || islandBridge.eventActive) return
            if (sourceId === "builtin.media" && targetId === mediaMonitor.selectedSessionId) {
                if ((actionId === "media.previous" && mediaMonitor.controls.previous)
                    || (actionId === "media.toggle" && mediaMonitor.controls.toggle)
                    || (actionId === "media.next" && mediaMonitor.controls.next)) mediaMonitor.invokeControl(actionId, targetId)
            } else if (sourceId === "builtin.focus" && actionId === "focus.toggle") focusTimer.toggle()
            else if (sourceId === "builtin.input") {
                if (actionId === "input.mute" && audioInputManager.available) audioInputManager.toggleMuted()
                else if (actionId === "input.settings") audioInputManager.openInputSettings()
            } else if (sourceId === "builtin.ocr" && actionId === "ocr.cancel" && clipboardOcr.busy) clipboardOcr.cancel()
            else if (sourceId === "builtin.awake" && actionId === "awake.stop" && awakeManager.active) awakeManager.stop()
        }
    }
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
            if (!root.mayLeaveEditor()) return
            const route=islandBridge.activeState.route
            if (route==="focus") sidebar.selectSection(2)
            else if (route==="notifications") sidebar.selectSection(1)
            else if (route==="microphone" || route==="media") sidebar.selectSection(0)
            else if (route==="clipboard") sidebar.selectSection(3)
            else if (["awake","usb","hotspot","network"].indexOf(route)>=0) sidebar.selectSection(0)
            else if (pluginRegistry.openRoute(route)) return
            else return
            root.showCards()
            if (["awake","usb","hotspot","network"].indexOf(route)>=0) Qt.callLater(() => sidebar.revealUtility(route))
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
        running: !previewCards && root.visible && root.shellState !== 0 && (uiSettings.autoHide || root.shellState === 1 || root.shellState === 3)
            && !islandBridge.eventActive && !dropdownController.pointerInside && !sidebar.transientUiOpen && !root.editing && !sidebar.interacting
        onTriggered: root.hidePanel()
    }
    Sidebar {
        id: sidebar
        z: 10
        x: (root.width-width)/2; y: nativeCardPreview ? 6 : 0
        width: root.panelWidth
        height: nativeCardPreview ? root.expandedHeight : root.height
        visible: root.shellState !== 0
        cardCanvas: desktopCanvas.item
        expanded: root.shellState === 2
        islandOnly: root.shellState === 3
        onExpandRequested: root.showCards()
        onCollapseRequested: root.collapseCards()
        onHideRequested: root.hidePanel()
    }
    Loader {
        id: desktopCanvas
        anchors.fill: parent
        active: nativeCardPreview && sidebar.nativeCards.ready
        sourceComponent: Component {
            DesktopCardCanvas {
                manager: uiCardManager
                componentProvider: sidebar.nativeCards.componentFor
                showAll: root.shellState === 2 && sidebar.currentSection === 0
                blocked: sidebar.visible && (sidebar.blockingPopupOpen || sidebar.islandItem.controlsOpen)
                headerArea: Qt.rect(sidebar.x,0,sidebar.width,sidebar.y+sidebar.bodyTop+8)
            }
        }
    }
    Shortcut { sequence: "Ctrl+Space"; onActivated: root.toggleBar() }
    Shortcut { sequence: "Escape"; enabled: !sidebar.blockingPopupOpen; onActivated: sidebar.islandItem.controlsOpen ? sidebar.islandItem.closeControls() : root.shellState === 2 ? root.collapseCards() : root.hidePanel() }
    Shortcut { sequence: "Ctrl+1"; onActivated: { if(root.mayLeaveEditor()) { sidebar.selectSection(0); root.showCards() } } }
    Shortcut { sequence: "Ctrl+2"; onActivated: { if(root.mayLeaveEditor()) { sidebar.selectSection(1); root.showCards() } } }
    Shortcut { sequence: "Ctrl+3"; onActivated: { if(root.mayLeaveEditor()) { sidebar.selectSection(2); root.showCards() } } }
    Shortcut { sequence: "Ctrl+4"; onActivated: { if(root.mayLeaveEditor()) { sidebar.selectSection(3); root.showCards() } } }
    Shortcut { sequence: "Ctrl+5"; onActivated: { if(root.mayLeaveEditor()) { sidebar.selectSection(4); root.showCards() } } }
    Shortcut { sequence: "Ctrl+Q"; onActivated: if(root.mayLeaveEditor()) Qt.quit() }
    onClosing: close=> { if(!mayLeaveEditor()) close.accepted=false }
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
                if (step === 6) {
                    if (!sidebar.verifyFeatureTestLayout("")) { console.error("UI_SELF_TEST feature layout failed"); Qt.exit(10); return }
                    sidebar.setFeatureTestState(false)
                    console.log("UI_SELF_TEST network/hotspot/awake/USB expanded states PASS")
                }
                // Check the previously laid-out focus page before switching away.
                if (step === 8 && root.width >= 620 && root.height >= 500) {
                    root.syncRegions()
                    if (sidebar.inputRegions().length < 8
                        || dropdownController.regionContains(Math.round(root.width/2), sidebar.bodyTop + 118)) {
                        console.error("UI_SELF_TEST independent focus cards failed"); Qt.exit(4); return
                    }
                }
                if (step === 9 && !sidebar.captureOcrTest("")) { console.error("UI_SELF_TEST OCR editor failed"); Qt.exit(10); return }
                sidebar.currentSection = step - 5
                if (step === 5) { sidebar.setFeatureTestState(true); sidebar.scheduleFeatureCapture(uiCapturePath) }
                if (step === 8) sidebar.scheduleOcrCapture(uiCapturePath)
                if (step === 9) sidebar.scheduleToolboxCapture(uiCapturePath)
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
                        {kind:"task"},
                        {kind:"task",mode:"event",tone:"success"},
                        {kind:"task",mode:"event",tone:"danger"},
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
