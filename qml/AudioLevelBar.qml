import QtQuick

// Recent real endpoint peaks, not FFT bins or generated animation.
Item {
    id: root
    property real level: 0
    property bool available: true
    property bool muted: false
    property color barColor: themeManager.textPrimary
    property int barCount: 25
    property var samples: []
    readonly property real safeLevel: available && !muted && isFinite(level) ? Math.max(0, Math.min(1, level)) : 0
    implicitWidth: 240
    implicitHeight: 76
    function clear() { samples = [] }
    function appendPeak() {
        const next = samples.slice(-(barCount - 1))
        next.push(safeLevel)
        samples = next
    }
    onMutedChanged: if (muted) clear()
    onAvailableChanged: if (!available) clear()
    onVisibleChanged: if (!visible) clear()
    Timer {
        interval: 100; repeat: true
        running: root.visible && root.available && !root.muted
        onTriggered: root.appendPeak()
    }
    Repeater {
        model: root.barCount
        Rectangle {
            required property int index
            readonly property int sampleIndex: index - (root.barCount - root.samples.length)
            readonly property real peak: root.available && !root.muted && sampleIndex >= 0 ? root.samples[sampleIndex] : 0
            width: Math.max(2, Math.min(7, root.width / root.barCount * 0.55))
            height: 2 + Math.sqrt(Math.max(0, peak)) * Math.max(0, root.height - 2)
            x: (index + 0.5) * root.width / root.barCount - width / 2
            y: (root.height - height) / 2
            color: root.barColor
            opacity: root.available ? 1 : 0.3
            Behavior on height { NumberAnimation { duration: 90 } }
            Behavior on y { NumberAnimation { duration: 90 } }
        }
    }
}
