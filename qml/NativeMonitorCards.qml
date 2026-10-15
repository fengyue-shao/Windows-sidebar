import QtQuick
import QtQuick.Layouts

Item {
    id: root
    required property var manager
    property bool ready: false
    // Component ownership and business bindings stay in QML, not the layout model.
    readonly property var definitions: [
        ["stats", "实时监控", 250], ["microphone", "麦克风", 200],
        ["disk", "磁盘", 210], ["network", "网络详情", 160],
        ["usb", "USB 存储", 160], ["controls", "系统控制", 188],
        ["hotspot", "移动热点", 170], ["awake", "保持唤醒", 180],
        ["media", "媒体", 246]
    ]
    function componentFor(id) {
        return ({"monitor.stats": stats, "monitor.microphone": microphone,
            "monitor.disk": disk, "monitor.controls": controls,
            "monitor.network": network, "monitor.usb": usb,
            "monitor.hotspot": hotspot, "monitor.awake": awake,
            "monitor.media": media})[id] || null
    }
    Component.onCompleted: {
        for (const d of definitions)
            manager.registerCard({id: "monitor."+d[0], page: "monitor", title: d[1],
                minimumWidth: 220, minimumHeight: 120, defaultHeight: d[2], maximumHeight: 4096})
        manager.setAvailable("monitor.media", uiSettings.mediaEnabled)
        ready = true
    }
    Connections {
        target: uiSettings
        function onMediaEnabledChanged() { root.manager.setAvailable("monitor.media", uiSettings.mediaEnabled) }
    }
    Component { id: microphone; MicrophoneCard {} }
    Component { id: disk; DiskPanel { model: diskMonitor } }
    Component { id: controls; QuickSettingsPanel {} }
    Component { id: network; NetworkDetailsCard {} }
    Component { id: usb; UsbCard {} }
    Component { id: hotspot; HotspotCard {} }
    Component { id: awake; AwakeCard {} }
    Component {
        id: media
        CardSurface {
            implicitHeight: 246
            MediaBar { id: bar; anchors.fill: parent; moduleEnabled: uiSettings.mediaEnabled }
            Text { anchors.centerIn: parent; visible: !bar.visible; text: "播放媒体后，在这里控制"; color: themeManager.textMuted; font.pixelSize: 12 }
        }
    }
    Component {
        id: stats
        CardSurface {
            implicitHeight: 250
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Text { text: "实时监控"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
                    SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "CPU"; value: cpuMonitor.usage.toFixed(0)+"%"; progress: cpuMonitor.usage/100; accent: themeManager.accent }
                    SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "内存"; value: memoryMonitor.usage.toFixed(0)+"%"; progress: memoryMonitor.usage/100; accent: themeManager.accentSecondary }
                    SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "GPU"; value: gpuMonitor.usage.toFixed(0)+"%"; progress: gpuMonitor.usage/100; accent: themeManager.warning }
                }
                Text { Layout.fillWidth: true; text: "↓ "+networkMonitor.downloadSpeedText+"    ↑ "+networkMonitor.uploadSpeedText; color: themeManager.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
                Text { text: systemTrayManager.online ? "网络已连接" : "网络未连接"; color: themeManager.textMuted; font.pixelSize: 11 }
            }
        }
    }
}
