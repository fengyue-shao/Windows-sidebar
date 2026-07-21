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
            GradientStop { position: 0.0; color: themeManager.mediaStart }
            GradientStop { position: 0.58; color: themeManager.mediaMiddle }
            GradientStop { position: 1.0; color: themeManager.mediaEnd }
        }

        border.width: 1
        border.color: themeManager.borderStrong

        RowLayout {
            anchors.fill: parent
            anchors.margins: 13
            spacing: 14

            Rectangle {
                id: coverFrame

                readonly property real sourceAspect: coverImage.status === Image.Ready
                                                     && coverImage.sourceSize.height > 0
                                                     ? coverImage.sourceSize.width / coverImage.sourceSize.height
                                                     : 1.0
                readonly property real fittedWidth: sourceAspect >= 1.0
                                                    ? Math.min(148, 106 * sourceAspect)
                                                    : Math.max(70, 106 * sourceAspect)
                readonly property real fittedHeight: sourceAspect >= 1.0
                                                     ? fittedWidth / sourceAspect
                                                     : 106

                Layout.preferredWidth: fittedWidth
                Layout.preferredHeight: fittedHeight
                Layout.alignment: Qt.AlignVCenter
                radius: 15
                color: themeManager.mediaCover
                clip: true

                Text {
                    anchors.centerIn: parent
                    text: "♪"
                    color: themeManager.accentSoft
                    font.pixelSize: 34
                }

                Image {
                    id: coverImage
                    anchors.fill: parent
                    source: "image://cover/current?" + mediaMonitor.coverVersion
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 28
                    gradient: Gradient {
                        GradientStop { position: 0; color: themeManager.overlayStart }
                        GradientStop { position: 1; color: themeManager.overlayEnd }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 7
                        text: mediaMonitor.playing ? "正在播放" : "已暂停"
                        color: themeManager.overlayText
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Text {
                        text: "MEDIA"
                        color: themeManager.accent
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        font.letterSpacing: 1.4
                    }

                    Rectangle {
                        Layout.preferredWidth: 3
                        Layout.preferredHeight: 3
                        radius: 2
                        color: themeManager.textFaint
                    }

                    MediaMarquee {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        text: mediaMonitor.provider || "未知来源"
                        textColor: themeManager.textMuted
                        pixelSize: 9
                        scrollPixelsPerSecond: 28
                    }
                }

                MediaMarquee {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 21
                    text: mediaMonitor.title
                    textColor: themeManager.textPrimary
                    pixelSize: 15
                    fontWeight: Font.DemiBold
                    scrollPixelsPerSecond: 38
                }

                MediaMarquee {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    text: mediaMonitor.artist || "未知艺术家"
                    textColor: themeManager.textSecondary
                    pixelSize: 11
                }

                MediaMarquee {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 15
                    visible: mediaMonitor.album.length > 0
                    text: mediaMonitor.album
                    textColor: themeManager.textMuted
                    pixelSize: 10
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 10
                        color: previousTap.pressed ? themeManager.accentSoft
                                                   : previousHover.hovered ? themeManager.surfaceHover : themeManager.surface

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
                        color: playTap.pressed ? themeManager.accent
                                              : playHover.hovered ? themeManager.accentHover : themeManager.accent

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
                        color: nextTap.pressed ? themeManager.accentSoft
                                               : nextHover.hovered ? themeManager.surfaceHover : themeManager.surface

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
