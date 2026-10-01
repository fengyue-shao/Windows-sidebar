import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

CardSurface {
    id: root
    property var backend: mediaMonitor
    property var spectrumBackend: mediaSpectrum
    readonly property alias sessionSelector: sessionList
    readonly property alias progressControl: progress
    readonly property bool hasMedia: root.backend.title.length > 0
    property bool moduleEnabled: true
    visible: moduleEnabled && (hasMedia || root.backend.sessions.length > 0)
    implicitHeight: 246
    radius: 18
    border.color: themeManager.borderStrong
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: themeManager.mediaStart }
        GradientStop { position: 0.58; color: themeManager.mediaMiddle }
        GradientStop { position: 1.0; color: themeManager.mediaEnd }
    }
    function timeText(seconds) {
        const value = Math.max(0, Math.floor(seconds))
        return Math.floor(value/60) + ":" + String(value%60).padStart(2,"0")
    }
    MediaSpectrumBars {
        anchors.right: parent.right; anchors.rightMargin: 18
        y: 60; width: Math.max(0, parent.width - 175); height: 125
        opacity: 0.16
        visible: root.spectrumBackend.requested
        active: root.spectrumBackend.enabled && root.spectrumBackend.available
        bands: root.spectrumBackend.bands
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 15; spacing: 6
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Basic.Button {
                text: "FFT"; implicitWidth: 40; implicitHeight: 30
                onClicked: root.spectrumBackend.requested = !root.spectrumBackend.requested
                contentItem: Text { text: parent.text; color: root.spectrumBackend.requested ? themeManager.accent : themeManager.textMuted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 9 }
                background: Rectangle { radius: 9; color: root.spectrumBackend.requested ? themeManager.accentSoft : themeManager.surfaceStrong }
                Basic.ToolTip.visible: hovered
                Basic.ToolTip.text: "输出频谱（默认关闭）\n分析默认输出的系统混音，包含其他应用声音\n仅在媒体页面展开且播放时分析，不读取麦克风，不保存或上传\n" + root.spectrumBackend.statusText
            }
            Basic.Button {
                text: "自动"; implicitWidth: 48; implicitHeight: 30
                enabled: root.backend.sessions.length > 0
                onClicked: root.backend.followSystemSession()
                contentItem: Text { text: parent.text; color: root.backend.followingSystem ? themeManager.accent : themeManager.textMuted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                background: Rectangle { radius: 9; color: root.backend.followingSystem ? themeManager.accentSoft : themeManager.surfaceStrong }
            }
            ListView {
                id: sessionList
                Layout.fillWidth: true; Layout.preferredHeight: 30
                orientation: ListView.Horizontal; spacing: 6; clip: true
                boundsBehavior: Flickable.StopAtBounds; model: root.backend.sessions
                delegate: Basic.Button {
                    required property var modelData
                    width: Math.min(132, Math.max(72, sessionLabel.implicitWidth+20)); height: 30
                    onClicked: root.backend.selectSession(modelData.id)
                    contentItem: Text { id: sessionLabel; text: modelData.name; elide: Text.ElideRight; color: modelData.selected ? themeManager.accent : themeManager.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 9; color: parent.modelData.selected ? themeManager.accentSoft : themeManager.surfaceStrong; border.width: 1; border.color: parent.modelData.selected ? themeManager.accent : themeManager.border }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
            Rectangle {
                readonly property real sourceAspect: coverImage.status === Image.Ready && coverImage.sourceSize.height > 0 ? coverImage.sourceSize.width / coverImage.sourceSize.height : 1
                readonly property real fittedWidth: sourceAspect >= 1 ? Math.min(154, 120 * sourceAspect) : Math.max(78, 120 * sourceAspect)
                Layout.preferredWidth: root.width < 340 ? 88 : fittedWidth
                Layout.preferredHeight: root.width < 340 ? 88 : sourceAspect >= 1 ? fittedWidth / sourceAspect : 120
                Layout.alignment: Qt.AlignVCenter
                radius: 15; color: themeManager.mediaCover; clip: true
                Text { anchors.centerIn: parent; text: "♪"; color: themeManager.accentSoft; font.pixelSize: 34 }
                Image { id: coverImage; anchors.fill: parent; fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false; source: root.hasMedia && root.backend.coverVersion > 0 ? "image://cover/current?"+root.backend.coverVersion : "" }
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 28
                    gradient: Gradient {
                        GradientStop { position: 0; color: themeManager.overlayStart }
                        GradientStop { position: 1; color: themeManager.overlayEnd }
                    }
                    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7; text: root.backend.playing ? "正在播放" : "已暂停"; color: themeManager.overlayText; font.pixelSize: 9; font.weight: Font.DemiBold }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 2
                RowLayout {
                    Layout.fillWidth: true; spacing: 7
                    Text { text: "MEDIA"; color: themeManager.accent; font.pixelSize: 9; font.weight: Font.Bold; font.letterSpacing: 1.4 }
                    Rectangle { Layout.preferredWidth: 3; Layout.preferredHeight: 3; radius: 2; color: themeManager.textFaint }
                    MediaMarquee { Layout.fillWidth: true; Layout.preferredHeight: 13; text: root.backend.provider || "未知来源"; textColor: themeManager.textMuted; pixelSize: 9 }
                }
                MediaMarquee { Layout.fillWidth: true; Layout.preferredHeight: 23; text: root.backend.title || "等待媒体信息"; textColor: themeManager.textPrimary; pixelSize: 16; fontWeight: Font.DemiBold }
                MediaMarquee { Layout.fillWidth: true; Layout.preferredHeight: 16; text: root.backend.artist || "未知艺术家"; textColor: themeManager.textSecondary; pixelSize: 11 }
                MediaMarquee { Layout.fillWidth: true; Layout.preferredHeight: 15; visible: text.length > 0; text: root.backend.album; textColor: themeManager.textMuted; pixelSize: 10 }
                Item { Layout.fillHeight: true }
                RowLayout {
                    spacing: 8
                    TransportButton { iconName: "previous"; tip: "上一首"; onClicked: root.backend.previous() }
                    TransportButton { primary: true; iconName: root.backend.playing ? "pause" : "play"; tip: root.backend.playing ? "暂停" : "播放"; onClicked: root.backend.togglePlayPause() }
                    TransportButton { iconName: "next"; tip: "下一首"; onClicked: root.backend.next() }
                }
            }
        }
        CardSlider {
            id: progress
            Layout.fillWidth: true; Layout.preferredHeight: 20
            from: 0; to: Math.max(1, root.backend.durationSeconds)
            enabled: root.backend.canSeek
            property string dragSession: ""
            property string dragTitle: ""
            property bool scrubbed: false
            property real dragPosition: 0
            onPressedChanged: {
                if (pressed) { dragSession = root.backend.selectedSessionId; dragTitle = root.backend.title; scrubbed = false }
                else if (scrubbed && enabled && dragSession === root.backend.selectedSessionId && dragTitle === root.backend.title) root.backend.seekTo(dragPosition, dragSession)
            }
            onMoved: {
                if (!enabled) return
                if (pressed) { scrubbed = true; dragPosition = value }
                else root.backend.seekTo(value, root.backend.selectedSessionId)
            }
            Binding { target: progress; property: "value"; value: root.backend.positionSeconds; when: !progress.pressed }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: root.timeText(progress.pressed ? progress.value : root.backend.positionSeconds); color: themeManager.textSecondary; font.pixelSize: 10 }
            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: root.backend.statusText || (root.spectrumBackend.requested ? "系统输出频谱 · " : "") + (root.backend.canSeek ? "拖动定位" : root.backend.durationSeconds > 0 ? "只读进度" : "未提供时间线"); elide: Text.ElideRight; color: themeManager.textFaint; font.pixelSize: 9 }
            Text { text: root.timeText(root.backend.durationSeconds); color: themeManager.textSecondary; font.pixelSize: 10 }
        }
    }
    component TransportButton: Basic.Button {
        property string iconName
        property string tip
        property bool primary: false
        implicitWidth: primary ? 40 : 32; implicitHeight: primary ? 40 : 32
        enabled: root.backend.sessions.length > 0
        contentItem: FluentIcon { name: parent.iconName; tint: parent.primary ? "white" : themeManager.accent }
        background: Rectangle { radius: parent.primary ? 13 : 10; color: parent.primary ? (parent.hovered ? themeManager.accentHover : themeManager.accent) : parent.hovered ? themeManager.surfaceHover : themeManager.surface }
        Basic.ToolTip.visible: hovered; Basic.ToolTip.text: tip
    }
}
