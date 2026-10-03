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
    // Target geometry stays deterministic for layout; only the rendered shell
    // interpolates. No bounce/overshoot may spill into neighbouring controls.
    property bool motionEnabled: true
    property bool controlsOpen: false
    property var controlData: ({actions: []})
    readonly property int headerHeight: hasActivity ? (isEvent ? 64 : 52) : 24
    signal actionRequested(string sourceId, string actionId, string targetId)
    function closeControls() { controlsOpen = false }
    readonly property bool motionRunning: widthMotion.running || heightMotion.running || contentReveal.running
    readonly property string contentKey: (activeState.sourceId || "") + ":" + kind + ":" + (activeState.mode || "") + ":" + (activeState.tone || "")
    function revealContent() {
        contentReveal.stop()
        activityContent.opacity = 1
        if (motionEnabled && visible && hasActivity) contentReveal.restart()
    }
    onContentKeyChanged: { closeControls(); revealContent() }
    onVisibleChanged: {
        if (visible) revealContent()
        else { widthMotion.complete(); heightMotion.complete(); contentReveal.stop(); activityContent.opacity = 1 }
    }
    onMotionEnabledChanged: if (!motionEnabled) { widthMotion.complete(); heightMotion.complete(); contentReveal.stop(); activityContent.opacity = 1 }
    implicitWidth: hasActivity ? Math.min(availableWidth, controlsOpen || isEvent ? 420 : 344) : 64
    implicitHeight: controlsOpen && hasActivity ? headerHeight + controlsColumn.implicitHeight + 24 : headerHeight
    width: implicitWidth; height: implicitHeight
    Behavior on width {
        enabled: root.motionEnabled && root.stateOverride === null && root.visible
        NumberAnimation { id: widthMotion; duration: 280; easing.type: Easing.OutCubic }
    }
    Behavior on height {
        enabled: root.motionEnabled && root.stateOverride === null && root.visible
        NumberAnimation { id: heightMotion; duration: 240; easing.type: Easing.OutCubic }
    }
    NumberAnimation {
        id: contentReveal; target: activityContent; property: "opacity"
        from: 0; to: 1; duration: 180; easing.type: Easing.OutQuad
    }
    radius: Math.min(32, height / 2); color: "#050506"
    border.width: 1; border.color: "#303036"; clip: true
    Accessible.name: hasActivity ? activeState.title + " " + (activeState.subtitle || "") + " " + detail : "灵动岛，双击展开卡片"
    Accessible.role: Accessible.Button
    Accessible.description: "单击展开岛内控制，双击展开或收起卡片；关闭提醒不取消任务"
    signal cardsToggleRequested()
    // Painted surfaces must also consume background input. Otherwise an empty
    // patch of the expanded island can activate the navigation underneath it.
    MouseArea {
        anchors.fill: parent; acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }
    Row {
        anchors.centerIn: parent; spacing: 5; visible: !root.hasActivity
        Repeater { model: 3; Rectangle { width: 4; height: 4; radius: 2; color: "#808088" } }
    }
    RowLayout {
        id: activityContent
        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
        anchors.topMargin: 9; anchors.bottomMargin: Math.max(9, root.height-root.headerHeight+9); spacing: 12; visible: root.hasActivity
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
                            running: root.motionEnabled && root.visible && root.hasActivity && root.kind === "terminal" && !root.isEvent
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
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: Math.max(5, root.height-root.headerHeight+5)
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
        anchors.bottom: parent.bottom; anchors.bottomMargin: Math.max(2, root.height-root.headerHeight+2); anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(0,parent.width-70)*(1-Math.max(0,Math.min(1,root.activeState.eventProgress || 0)))
        height: 1; color: root.accent; opacity: 0.45
    }
    Item {
        width: parent.width-(root.isEvent ? 52 : 0); height: root.headerHeight
        TapHandler {
            exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
            onDoubleTapped: { root.closeControls(); root.cardsToggleRequested() }
            onSingleTapped: if (root.hasActivity) root.controlsOpen = !root.controlsOpen
        }
    }
    ColumnLayout {
        id: controlsColumn
        objectName: "islandControls"
        x: 18; y: root.headerHeight+8; width: Math.max(0, root.width-36)
        spacing: 10; visible: root.controlsOpen && root.hasActivity
        Text {
            Layout.fillWidth: true; text: root.controlData.status || root.activeState.subtitle || "查看来源卡片获取完整信息"
            color: "#B5B5BE"; font.pixelSize: 11; textFormat: Text.PlainText
            wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            Repeater {
                model: root.controlData.actions || []
                IslandButton {
                    required property var modelData
                    objectName: "islandAction_" + modelData.id
                    text: modelData.label; enabled: modelData.enabled !== false
                    onClicked: root.actionRequested(root.activeState.sourceId, modelData.id, root.controlData.targetId || "")
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            IslandButton {
                objectName: "islandDetails"; text: "打开来源卡片"
                onClicked: { root.closeControls(); islandBridge.requestDetails() }
            }
            Item { Layout.fillWidth: true }
            IslandButton { objectName: "islandCollapse"; text: "收起"; onClicked: root.closeControls() }
        }
    }
    component IslandButton: Basic.Button {
        implicitWidth: Math.max(68, contentItem.implicitWidth+24); implicitHeight: 32
        focusPolicy: Qt.TabFocus
        contentItem: Text { text: parent.text; color: parent.enabled ? "#E6E6EF" : "#676770"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 12; color: parent.hovered || parent.activeFocus ? "#30303A" : "#1D1D25"; border.width: parent.activeFocus ? 1 : 0; border.color: root.accent }
    }
}
