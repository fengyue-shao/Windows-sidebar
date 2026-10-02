import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root
    contentWidth: width; contentHeight: columns.implicitHeight
    clip: true; boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
    readonly property bool wide: width >= 620
    function revealUtility(route) {
        const card=({network:connectionDetails,hotspot:hotspot,awake:awake,usb:usb})[route]
        if (!card) return
        Qt.callLater(function() {
            const position=card.mapToItem(root.contentItem,0,0)
            root.contentY=Math.max(0,Math.min(position.y,Math.max(0,root.contentHeight-root.height)))
        })
    }
    function scheduleFeatureCapture(path) { featureCapture.path = path; featureCapture.restart() }
    Timer {
        id: featureCapture
        property string path: ""
        interval: 80
        onTriggered: root.verifyFeatureTestLayout(path)
    }
    function setFeatureTestState(enabled) {
        if (!uiSelfTest) return
        connectionDetails.expanded = enabled
        hotspot.expanded = enabled
        hotspot.pending = enabled ? "configure" : ""
        awake.confirming = enabled
        usb.pendingId = enabled ? "test-only-no-eject" : ""
        usb.pendingName = "USB 测试存储 · X:\\ · Y:\\"
        if (enabled) {
            connectionDetails.connectionSummary = "测试连接 · 系统报告互联网连接 · 按流量计费"
            connectionDetails.adapterItems = [{name: "测试适配器", description: "只用于 UI 验收的虚拟数据", addresses: "192.0.2.10/24\n2001:db8::10/64", gateways: "192.0.2.1", dns: "192.0.2.53", mtu: 1500, speed: "↓ 1000 / ↑ 1000 Mbps"}]
            usb.deviceItems = [{id: "test-only-no-eject", name: "USB 测试存储", volumes: "X:\\ · Y:\\"}]
        } else {
            connectionDetails.connectionSummary = Qt.binding(() => networkDetails.summary)
            connectionDetails.adapterItems = Qt.binding(() => networkDetails.adapters)
            usb.deviceItems = Qt.binding(() => usbManager.devices)
            contentY = 0
        }
    }
    function verifyFeatureTestLayout(capturePath) {
        if (!uiSelfTest) return false
        for (const item of [connectionDetails, hotspot, awake, usb]) {
            if (!Number.isFinite(item.height) || item.height <= 100 || item.width <= 0) return false
        }
        if (contentHeight < connectionDetails.y + connectionDetails.height) return false
        contentY = Math.max(0, Math.min(connectionDetails.y, contentHeight - height))
        if (capturePath.length) root.grabToImage(result => result.saveToFile(capturePath + ".features.png"))
        return true
    }
    function inputRegions(target) {
        const vp = root.mapToItem(target, 0, 0)
        let result = []
        for (const item of [stats, disk, controls, microphone, media, connectionDetails, hotspot, awake, usb]) {
            if (!item.visible) continue
            const p = item.mapToItem(target, 0, 0)
            const x = Math.max(vp.x, p.x), y = Math.max(vp.y, p.y)
            const right = Math.min(vp.x + root.width, p.x + item.width), bottom = Math.min(vp.y + root.height, p.y + item.height)
            if (right > x && bottom > y) result.push({x: x, y: y, width: right-x, height: bottom-y, radius: y === p.y && bottom === p.y+item.height ? 16 : 0})
        }
        return result
    }
    GridLayout {
        id: columns
        width: root.width; columns: root.wide ? 2 : 1; columnSpacing: 16; rowSpacing: 16
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: root.wide ? (root.width-16)/2 : root.width
            Layout.alignment: Qt.AlignTop; spacing: 16
            Rectangle {
                id: stats
                Layout.fillWidth: true; Layout.preferredHeight: 250; radius: 16
                color: themeManager.cardSurface; border.width: 1; border.color: themeManager.border
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: "实时监控"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
                        Text { text: "● 实时"; color: themeManager.success; font.pixelSize: 10 }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
                        SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "CPU"; value: cpuMonitor.usage.toFixed(0)+"%"; progress: cpuMonitor.usage/100; accent: themeManager.accent }
                        SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "内存"; value: memoryMonitor.usage.toFixed(0)+"%"; progress: memoryMonitor.usage/100; accent: themeManager.accentSecondary }
                        SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "GPU"; value: gpuMonitor.usage.toFixed(0)+"%"; progress: gpuMonitor.usage/100; accent: themeManager.warning }
                    }
                    Text { Layout.fillWidth: true; text: "↓ "+networkMonitor.downloadSpeedText+"    ↑ "+networkMonitor.uploadSpeedText; color: themeManager.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: systemTrayManager.online ? "网络已连接" : "网络未连接"; color: themeManager.textMuted; font.pixelSize: 11 }
                }
            }
            MicrophoneCard { id: microphone; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight }
            DiskPanel { id: disk; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight; model: diskMonitor }
            NetworkDetailsCard { id: connectionDetails; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight }
            UsbCard { id: usb; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: root.wide ? (root.width-16)/2 : root.width
            Layout.alignment: Qt.AlignTop; spacing: 16
            QuickSettingsPanel { id: controls; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight }
            HotspotCard { id: hotspot; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight }
            AwakeCard { id: awake; Layout.fillWidth: true; Layout.preferredHeight: implicitHeight }
            Rectangle {
                id: media
                Layout.fillWidth: true; Layout.preferredHeight: mediaCard.implicitHeight; visible: uiSettings.mediaEnabled
                radius: 16; color: themeManager.cardSurface; border.width: 1; border.color: themeManager.border
                MediaBar { id: mediaCard; anchors.fill: parent; moduleEnabled: uiSettings.mediaEnabled }
                Column {
                    anchors.centerIn: parent; spacing: 10; visible: !mediaCard.visible
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "正在播放"; color: themeManager.textPrimary; font.pixelSize: 16 }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "播放媒体后，在这里控制"; color: themeManager.textMuted; font.pixelSize: 12 }
                }
            }
        }
    }
}
