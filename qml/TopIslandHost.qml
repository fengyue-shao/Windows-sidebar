import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic

Rectangle {
    id: root
    // Presentation-only fixtures do not start audio streams.
    property var stateOverride: null
    readonly property var activeState: stateOverride === null ? islandBridge.activeState : stateOverride
    readonly property bool hasActivity: !!activeState.sourceId
    readonly property bool isEvent: activeState.mode === "event"
    readonly property string kind: activeState.kind || ({microphone:"input",media:"media",focus:"focus",notifications:"notification",terminal:"terminal",awake:"awake",clipboard:"ocr",usb:"usb",hotspot:"network"})[activeState.route] || ""
    readonly property bool inputActive: hasActivity && kind === "input" && !isEvent
    readonly property bool builtinMedia: hasActivity && kind === "media" && !isEvent && activeState.sourceId === "builtin.media"
    readonly property color accent: activeState.tone === "danger" ? "#FF939A"
        : activeState.tone === "success" || kind === "input" ? "#80DAB2"
        : kind === "focus" ? "#B7A4FF" : kind === "awake" ? "#F5CB80" : "#8CCBFF"
    readonly property int pendingCount: Math.max(0, (activeState.queuedCount || 0) - (isEvent ? 1 : 0))
    readonly property bool hasProgress: hasActivity && activeState.progress !== undefined && isFinite(activeState.progress)
    readonly property string detail: activeState.detailText || (inputActive && activeState.applicationCount > 1 ? String(activeState.applicationCount) : "")
    readonly property bool inputMeterVisible: inputActive && audioInputManager.inputMeterAvailable && !audioInputManager.muted
    readonly property bool spectrumVisible: builtinMedia && mediaSpectrum.requested && mediaSpectrum.enabled && mediaSpectrum.available
    property real availableWidth: 420
    implicitWidth: hasActivity ? Math.min(availableWidth, isEvent ? 420 : 344) : 64
    implicitHeight: hasActivity ? (isEvent ? 64 : 52) : 24
    width: implicitWidth; height: implicitHeight
    radius: height / 2; color: "#050506"
    border.width: 1; border.color: "#303036"; clip: true
    Accessible.name: hasActivity ? activeState.title + " " + (activeState.subtitle || "") + " " + detail : "灵动岛，双击展开卡片"
    Accessible.role: Accessible.Button
    Accessible.description: isEvent ? "单击查看详情，右侧关闭只结束本次提醒，不取消任务或删除通知" : "单击查看详情，双击展开或收起卡片"
    signal cardsToggleRequested()
    Row {
        anchors.centerIn: parent; spacing: 5; visible: !root.hasActivity
        Repeater { model: 3; Rectangle { width: 4; height: 4; radius: 2; color: "#808088" } }
    }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
        anchors.topMargin: 9; anchors.bottomMargin: 9; spacing: 12; visible: root.hasActivity
        Rectangle {
            Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 16; color: "#1C2730"
            Image {
                id: cover
                anchors.fill: parent; anchors.margins: 2
                fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false
                source: root.builtinMedia && mediaMonitor.coverVersion > 0 ? "image://cover/current?" + mediaMonitor.coverVersion : ""
                visible: status === Image.Ready
            }
            FluentIcon {
                anchors.centerIn: parent; width: 19; height: 19; visible: !cover.visible
                name: root.activeState.iconName || "timer"; tint: root.accent
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 3
            Text {
                objectName: "islandTitle"
                Layout.fillWidth: true; text: root.activeState.title || ""; color: "#FFFFFF"
                textFormat: Text.PlainText
                font.family: "Segoe UI"; font.pixelSize: 12; elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true; visible: text.length > 0
                text: root.activeState.subtitle || ""; color: "#B5B5BE"
                textFormat: Text.PlainText
                font.family: "Segoe UI"; font.pixelSize: 10; elide: Text.ElideRight
            }
        }
        Item {
            Layout.preferredWidth: root.detail.length > 0 ? 52 : 64
            Layout.preferredHeight: 28
            visible: root.detail.length > 0 || root.inputMeterVisible || root.spectrumVisible || (root.kind === "terminal" && !root.isEvent)
            Text {
                anchors.fill: parent; visible: root.detail.length > 0
                text: root.detail; color: root.accent; font.family: "Segoe UI"; font.pixelSize: 14
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            AudioLevelBar {
                anchors.fill: parent; visible: root.inputMeterVisible && !root.detail.length
                barCount: 13; barColor: root.accent
                level: audioInputManager.inputPeak
                available: visible && audioInputManager.inputMeterAvailable; muted: audioInputManager.muted
            }
            MediaSpectrumBars {
                anchors.fill: parent; visible: root.spectrumVisible
                active: visible; bands: mediaSpectrum.bands; barColor: root.accent
            }
            Row {
                anchors.centerIn: parent
                visible: root.kind === "terminal" && !root.isEvent && !root.detail.length
                spacing: 5
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        width: 4; height: 4; radius: 2; color: root.accent; opacity: 0.4
                        SequentialAnimation on opacity {
                            running: root.visible && root.hasActivity && root.kind === "terminal" && !root.isEvent
                            loops: Animation.Infinite
                            PauseAnimation { duration: index * 120 }
                            NumberAnimation { to: 1; duration: 280 }
                            NumberAnimation { to: 0.4; duration: 280 }
                            PauseAnimation { duration: (2 - index) * 120 }
                        }
                    }
                }
            }
        }
        Rectangle {
            visible: root.pendingCount > 0
            Layout.preferredWidth: 28; Layout.preferredHeight: 20; radius: 10; color: "#25252C"
            Text { anchors.centerIn: parent; text: "+" + root.pendingCount; color: "#FFFFFF"; font.pixelSize: 10 }
        }
        Basic.ToolButton {
            objectName: "islandDismiss"
            visible: root.isEvent; Layout.preferredWidth: 22; Layout.preferredHeight: 22
            text: "×"; focusPolicy: Qt.TabFocus
            Accessible.name: "关闭本次灵动岛提醒"
            background: Rectangle { radius: 11; color: parent.hovered || parent.activeFocus ? "#35353D" : "#202027" }
            contentItem: Text { text: "×"; color: "#CFCFD7"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            onClicked: islandBridge.dismissCurrentEvent()
            Basic.ToolTip.visible: hovered
            Basic.ToolTip.text: "只关闭提醒，不取消任务"
        }
    }
    Rectangle {
        objectName: "islandProgress"
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 5
        width: Math.max(0, parent.width - 100); height: 2; radius: 1; color: "#303036"
        visible: root.hasProgress
        Rectangle {
            height: parent.height; width: parent.width * Math.max(0, Math.min(1, root.activeState.progress || 0))
            radius: 1; color: root.accent
        }
    }
    Rectangle {
        objectName: "islandEventLifetime"
        visible: root.isEvent && root.activeState.eventProgress !== undefined
        anchors.bottom: parent.bottom; anchors.bottomMargin: 2; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(0,parent.width-70)*(1-Math.max(0,Math.min(1,root.activeState.eventProgress || 0)))
        height: 1; color: root.accent; opacity: 0.45
    }
    Item {
        anchors.fill: parent; anchors.rightMargin: root.isEvent ? 52 : 0
        TapHandler {
            exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
            onDoubleTapped: root.cardsToggleRequested()
            onSingleTapped: islandBridge.requestDetails()
        }
    }
}
