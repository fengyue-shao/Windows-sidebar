import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 384
    height: 1080
    x:1536
    y:0
    visible: true

    flags: Qt.FramelessWindowHint
         | Qt.WindowStaysOnTopHint

    color: "transparent"

    MouseArea {
        anchors.fill: parent

        property point clickPos

        onPressed: function(mouse) {
            clickPos = Qt.point(mouse.x, mouse.y)
        }

        onPositionChanged: function(mouse) {
            if (mouse.buttons & Qt.LeftButton) {
                root.x += mouse.x - clickPos.x
                root.y += mouse.y - clickPos.y
            }
        }
    }
    Rectangle {
        anchors.fill: parent

        radius: 0

        color: "#AA202020"

        Timer {
            id: clockTimer

            interval: 1000

            running: true

            repeat: true

            onTriggered: {
                clockText.text =
                    Qt.formatTime(
                        new Date(),
                        "hh:mm:ss")
            }
        }

        Column {

                    anchors.centerIn: parent

                    spacing: 12

                    Text {
                        text:
                            "CPU : "
                            + cpuMonitor.usage.toFixed(0)
                            + "%"

                        color: "white"
                    }
                    Text{
                        text:
                            "Memory : "
                            + memoryMonitor.usage.toFixed(0)
                            + "%"
                        color:  "white"
                    }

                    Text {
                        text:
                            "UploadSpeed :"+networkMonitor.uploadSpeedText
                        color: "white"
                    }

                    Text{
                        text:"DownloadSpeed :"+networkMonitor.downloadSpeedText
                        color:"white"
                    }

                    Text {
                        id:clockText
                        text:
                            "CurrentTime :"+Qt.formatTime(
                                new Date(),
                                "hh:mm:ss")

                        color: "white"
                    }
        }
    }
}
