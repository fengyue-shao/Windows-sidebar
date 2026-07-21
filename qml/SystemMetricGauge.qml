import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string label
    property string value
    property real progress: 0
    property color accent: themeManager.accent

    radius: 16
    color: themeManager.surface
    border.width: 1
    border.color: themeManager.border

    Canvas {
        id: gauge
        anchors.centerIn: parent
        width: Math.min(parent.width - 18, parent.height - 18)
        height: width

        onPaint: {
            const ctx = getContext("2d")
            const center = width / 2
            const lineWidth = Math.max(5, width * 0.075)
            const radius = center - lineWidth
            const start = Math.PI * 0.75
            const sweep = Math.PI * 1.5
            ctx.reset()
            ctx.lineWidth = lineWidth
            ctx.lineCap = "round"
            ctx.strokeStyle = themeManager.track
            ctx.beginPath()
            ctx.arc(center, center, radius, start, start + sweep)
            ctx.stroke()
            ctx.strokeStyle = root.accent
            ctx.beginPath()
            ctx.arc(center, center, radius, start,
                    start + sweep * Math.max(0, Math.min(1, root.progress)))
            ctx.stroke()
        }

        Connections {
            target: root
            function onProgressChanged() { gauge.requestPaint() }
            function onAccentChanged() { gauge.requestPaint() }
        }
        Connections {
            target: themeManager
            function onThemeChanged() { gauge.requestPaint() }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 1

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.value
            color: themeManager.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: themeManager.textMuted
            font.pixelSize: 10
        }
    }
}
