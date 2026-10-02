import QtQuick
QtObject {
    id: root
    property int completedSerial: 0
    function shortEvent(source, title, subtitle, kind, route, icon, priority, tone) {
        islandBridge.pushEvent(source,"result-"+(++root.completedSerial),{title:title,subtitle:subtitle,kind:kind,route:route,iconName:icon,tone:tone||"neutral"},priority,4500,20000)
    }
    function updateAwake() {
        if (!awakeManager.active) { islandBridge.withdraw("builtin.awake"); return }
        const seconds=awakeManager.remainingSeconds
        const state={title:"保持唤醒",subtitle:awakeManager.keepDisplay ? "系统与屏幕" : "仅系统 · 屏幕可休眠",detailText:Math.floor(seconds/60)+":"+String(seconds%60).padStart(2,"0"),iconName:"timer",route:"awake",kind:"awake"}
        if (awakeManager.durationSeconds>0) state.progress=1-seconds/awakeManager.durationSeconds
        islandBridge.publishState("builtin.awake",state,10)
    }
    function updateOcr() {
        if (!clipboardOcr.busy) { islandBridge.withdraw("builtin.ocr"); return }
        islandBridge.publishState("builtin.ocr",{title:"正在本地识别图片",subtitle:"不上传图片 · 点击查看",kind:"ocr",route:"clipboard",iconName:"clipboard"},15)
    }
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
    property Connections awakeConnection: Connections {
        target:awakeManager
        function onChanged() { root.updateAwake() }
        function onExpired() { root.shortEvent("builtin.awake.done","保持唤醒已到期","已释放请求，恢复系统电源策略","awake","awake","checkmark",90,"success") }
    }
    property Connections ocrConnection: Connections {
        target:clipboardOcr
        function onChanged() { root.updateOcr() }
        function onCompleted(hasText) { root.shortEvent("builtin.ocr.done",hasText ? "图片文字识别完成" : "图片识别已结束","点击查看结果与状态","ocr","clipboard","clipboard",75,hasText ? "success" : "neutral") }
    }
    property Connections usbConnection: Connections {
        target:usbManager
        function onOperationFinished(success) { root.shortEvent("builtin.usb.done",success ? "Windows 已接受安全弹出" : "USB 安全弹出未完成",success ? "确认设备消失后再拔出" : "点击查看系统拒绝原因","usb","usb",success ? "checkmark" : "alert",success ? 85 : 95,success ? "success" : "danger") }
    }
    property Connections hotspotConnection: Connections {
        target:hotspotManager
        function onOperationCompleted() { root.shortEvent("builtin.hotspot.done","热点操作已结束","点击查看系统返回的状态","network","hotspot","wifi_1",80,"neutral") }
    }
    Component.onCompleted: { updateInput(); updateMedia(); updateFocus(); updateAwake(); updateOcr() }
}
