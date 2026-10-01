import QtQuick

CardSurface {
    id: root
    property string timeText
    property bool running
    property real progress
    implicitHeight: 280
    FluentIcon { x: 18; y: 15; name: "timer"; tint: themeManager.accent }
    Text { x: 46; y: 16; text: "当前专注"; color: themeManager.textSecondary; font.pixelSize: 13 }
    Item {
        width: 206; height: 206; anchors.centerIn: parent; anchors.verticalCenterOffset: 10
        Canvas {
            id: ring
            anchors.fill: parent
            property real progress: root.progress
            property color trackColor: themeManager.track
            property color progressColor: themeManager.accent
            onProgressChanged: requestPaint()
            onTrackColorChanged: requestPaint()
            onProgressColorChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d"), center = width/2, radius = center-10
                ctx.reset(); ctx.lineWidth = 8; ctx.lineCap = "round"
                ctx.strokeStyle = trackColor; ctx.beginPath(); ctx.arc(center, center, radius, 0, Math.PI*2); ctx.stroke()
                if (progress > 0) {
                    ctx.strokeStyle = progressColor; ctx.beginPath()
                    ctx.arc(center, center, radius, -Math.PI/2, -Math.PI/2+Math.PI*2*progress); ctx.stroke()
                }
            }
        }
        Column {
            anchors.centerIn: parent; spacing: 7
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.timeText; color: themeManager.textPrimary; font.pixelSize: 40; font.weight: Font.Light }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.running ? "进行中" : "准备就绪"; color: root.running ? themeManager.success : themeManager.textMuted; font.pixelSize: 11 }
        }
    }
}
