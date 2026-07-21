import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    readonly property bool hasMedia: mediaMonitor.title.length > 0
    visible: hasMedia

    Rectangle {
        anchors.fill: parent
        radius: 18
        clip: true

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#302D4264" }
            GradientStop { position: 0.58; color: "#E8222732" }
            GradientStop { position: 1.0; color: "#F21B1D23" }
        }

        border.width: 1
        border.color: "#28FFFFFF"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 13
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 106
                Layout.preferredHeight: 106
                radius: 15
                color: "#18202730"
                clip: true

                Text {
                    anchors.centerIn: parent
                    text: "♪"
                    color: "#456EA9E8"
                    font.pixelSize: 34
                }

                Image {
                    anchors.fill: parent
                    source: "image://cover/current?" + mediaMonitor.coverVersion
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 28
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#00000000" }
                        GradientStop { position: 1; color: "#B0000000" }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 7
                        text: mediaMonitor.playing ? "正在播放" : "已暂停"
                        color: "#E8FFFFFF"
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 2

                Text {
                    text: "MEDIA"
                    color: "#728FBEF2"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    font.letterSpacing: 1.4
                }

                Text {
                    Layout.fillWidth: true
                    text: mediaMonitor.title
                    color: "#F7FFFFFF"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: mediaMonitor.artist || "未知艺术家"
                    color: "#9FFFFFFF"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 10
                        color: previousTap.pressed ? "#365D83AE" : previousHover.hovered ? "#24FFFFFF" : "#13FFFFFF"

                        Image {
                            anchors.centerIn: parent
                            width: 17; height: 17
                            source: Qt.resolvedUrl("../assets/icons/ic_fluent_previous_48_regular.svg")
                        }
                        HoverHandler { id: previousHover }
                        TapHandler { id: previousTap; onTapped: mediaMonitor.previous() }
                        ToolTip.visible: previousHover.hovered
                        ToolTip.text: "上一首"
                    }

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 13
                        color: playTap.pressed ? "#C76BA7EA" : playHover.hovered ? "#FF79B9FF" : "#EA68AEFA"

                        Image {
                            anchors.centerIn: parent
                            width: 19; height: 19
                            source: mediaMonitor.playing
                                    ? Qt.resolvedUrl("../assets/icons/ic_fluent_pause_48_regular.svg")
                                    : Qt.resolvedUrl("../assets/icons/ic_fluent_play_48_regular.svg")
                        }
                        HoverHandler { id: playHover }
                        TapHandler { id: playTap; onTapped: mediaMonitor.togglePlayPause() }
                        ToolTip.visible: playHover.hovered
                        ToolTip.text: mediaMonitor.playing ? "暂停" : "播放"
                    }

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 10
                        color: nextTap.pressed ? "#365D83AE" : nextHover.hovered ? "#24FFFFFF" : "#13FFFFFF"

                        Image {
                            anchors.centerIn: parent
                            width: 17; height: 17
                            source: Qt.resolvedUrl("../assets/icons/ic_fluent_next_48_regular.svg")
                        }
                        HoverHandler { id: nextHover }
                        TapHandler { id: nextTap; onTapped: mediaMonitor.next() }
                        ToolTip.visible: nextHover.hovered
                        ToolTip.text: "下一首"
                    }
                }
            }
        }
    }
}
