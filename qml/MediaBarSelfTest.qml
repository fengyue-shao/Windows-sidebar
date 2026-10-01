import QtQuick

Item {
    id: root
    implicitWidth: 360; implicitHeight: 246
    QtObject {
        id: fake
        property var sessions: [{id:"a",name:"播放器 A",selected:true},{id:"b",name:"播放器 B",selected:false}]
        property string selectedSessionId: "a"
        property bool followingSystem: true
        property string title: "媒体会话与可拖动进度测试"
        property string artist: "本地模拟数据，不控制真实播放器"
        property string album: ""
        property string provider: "测试"
        property int coverVersion: 0
        property bool playing: false
        property real positionSeconds: 12
        property real durationSeconds: 120
        property bool canSeek: true
        property string statusText: ""
        property int seekCount: 0
        property real lastSeek: -1
        function selectSession(id) { selectedSessionId = id; followingSystem = false }
        function followSystemSession() { followingSystem = true }
        function seekTo(seconds,id) { if (id === selectedSessionId) { seekCount++; lastSeek = seconds; positionSeconds = seconds } }
        function previous() {}
        function next() {}
        function togglePlayPause() { playing = !playing }
    }
    QtObject {
        id: fakeSpectrum
        property bool requested: true
        property bool enabled: true
        property bool available: true
        // Static rendering fixture only, never a production audio source.
        property var bands: [0.1,0.15,0.25,0.6,0.95,0.72,0.4,0.28,0.12,0.04,0.02,0.12,0.2,0.3,0.42,0.3,0.16,0.07,0.02,0.04,0.1,0.22,0.3,0.5,0.76,0.95,0.66,0.34,0.2,0.14,0.08,0.04]
        property string statusText: "视觉测试频段，不读取真实声音"
    }
    MediaBar { id: card; anchors.fill: parent; backend: fake; spectrumBackend: fakeSpectrum }
    MediaSpectrumBars { id: spectrumTest; visible: false; active: true; bands: [Number.NaN,2,-1] }
    AudioLevelBar { id: meter; visible: false }
    function runTests() {
        card.sessionSelector.forceLayout()
        const second = card.sessionSelector.itemAtIndex(1)
        if (!second) throw Error("media session delegate missing")
        second.clicked()
        if (fake.selectedSessionId !== "b" || fake.followingSystem) throw Error("media selection routing")
        const slider = card.progressControl
        slider.pressed = true; slider.value = 35; slider.moved(); slider.pressed = false
        if (fake.seekCount !== 1 || fake.lastSeek !== 35) throw Error("media scrub routing")
        slider.pressed = true; slider.value = 45; slider.moved(); fake.selectedSessionId = "a"; slider.pressed = false
        if (fake.seekCount !== 1) throw Error("stale session scrub")
        fake.canSeek = false; slider.value = 60; slider.moved()
        if (fake.seekCount !== 1 || slider.enabled) throw Error("unsupported seek")
        meter.level = Number.NaN; if (meter.safeLevel !== 0) throw Error("meter NaN")
        meter.level = 2; if (meter.safeLevel !== 1) throw Error("meter clamp")
        meter.muted = true; if (meter.safeLevel !== 0) throw Error("meter mute")
        meter.muted = false; meter.level = 0.25; meter.appendPeak()
        if (meter.samples.length !== 1 || meter.samples[0] !== 0.25) throw Error("waveform real history")
        for (let i = 0; i < 40; ++i) meter.appendPeak()
        if (meter.samples.length !== meter.barCount) throw Error("waveform bounded history")
        meter.muted = true
        if (meter.samples.length !== 0) throw Error("waveform mute clear")
        if (spectrumTest.barItems.itemAt(0).value !== 0 || spectrumTest.barItems.itemAt(1).value !== 1 || spectrumTest.barItems.itemAt(2).value !== 0) throw Error("spectrum visual bounds")
        spectrumTest.active = false
        if (spectrumTest.barItems.itemAt(1).value !== 0) throw Error("spectrum inactive")
        fake.canSeek = true
        console.info("UI_SELF_TEST media selection + scrub + stale session + read-only + meter bounds PASS")
    }
    function capture(path,done) {
        card.grabToImage(result => {
            if (!result.saveToFile(path)) { console.error("UI_SELF_TEST capture failed"); Qt.exit(6); return }
            done()
        })
    }
}
