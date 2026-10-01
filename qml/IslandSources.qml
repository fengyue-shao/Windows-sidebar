import QtQuick
QtObject {
    id: root
    property int completedSerial: 0
    function updateInput() {
        const apps=captureUsageMonitor.applications
        if (apps.length===0) { islandBridge.withdraw("builtin.input"); return }
        const names=apps.map(app=>app.name).join(" · ")
        islandBridge.publishState("builtin.input",{title:apps.length===1 ? apps[0].name+" · 麦克风使用中" : apps.length+" 个应用正在使用麦克风",subtitle:apps.length===1 ? "音频输入 · 当前应用" : names,iconName:"mic",route:"microphone",kind:"input",applicationCount:apps.length},60)
    }
    function updateMedia() {
        if (!uiSettings.mediaEnabled || !mediaMonitor.playing) { islandBridge.withdraw("builtin.media"); return }
        const state={title:mediaMonitor.title||"媒体播放中",subtitle:mediaMonitor.artist||mediaMonitor.provider,iconName:"play",route:"media",kind:"media"}
        // A missing timeline is not a real 0% progress value.
        if (mediaMonitor.durationSeconds>0) state.progress=mediaMonitor.positionSeconds/mediaMonitor.durationSeconds
        islandBridge.publishState("builtin.media",state,40)
    }
    function updateFocus() {
        if (!focusTimer.running) { islandBridge.withdraw("builtin.focus"); return }
        const seconds=focusTimer.remainingSeconds
        islandBridge.publishState("builtin.focus",{title:"专注进行中",subtitle:"剩余时间",detailText:Math.floor(seconds/60)+":"+String(seconds%60).padStart(2,"0"),iconName:"timer",route:"focus",kind:"focus",progress:focusTimer.progress},20)
    }
    property Connections inputConnection: Connections { target:captureUsageMonitor; function onUsageChanged() { root.updateInput() } }
    property Connections mediaConnection: Connections {
        target:mediaMonitor
        function onMediaChanged() { root.updateMedia() }
        function onTimelineChanged() { root.updateMedia() }
    }
    property Connections settingsConnection: Connections { target:uiSettings; function onMediaEnabledChanged() { root.updateMedia() } }
    property Connections focusConnection: Connections {
        target:focusTimer
        function onRunningChanged() { root.updateFocus() }
        function onRemainingSecondsChanged() { root.updateFocus() }
        function onSessionCompleted() { islandBridge.pushEvent("builtin.focus.done","session-"+(++root.completedSerial),{title:"这一轮专注完成了",subtitle:"休息一下 · 点击回到专注",iconName:"checkmark",route:"focus",kind:"focus",tone:"success"},90,5000,20000) }
    }
    Component.onCompleted: { updateInput(); updateMedia(); updateFocus() }
}
