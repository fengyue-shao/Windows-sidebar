import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property int currentPanel: 0
    signal popupVisibilityChanged(bool visible)

    function openPanel(index) {
        currentPanel = index
        controlPopup.open()
    }

    implicitHeight: systemTrayManager.batteryPresent ? 188 : 164
    radius: 16
    color: themeManager.surface
    border.width: 1
    border.color: themeManager.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 9
            Rectangle {
                Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 10
                color: muteHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
                Text { anchors.centerIn: parent; text: systemTrayManager.muted || systemTrayManager.volume === 0 ? "×" : "♪"; color: systemTrayManager.muted ? themeManager.danger : themeManager.accent; font.pixelSize: 15; font.weight: Font.Bold }
                HoverHandler { id: muteHover }
                TapHandler { onTapped: systemTrayManager.toggleMuted() }
                ToolTip.visible: muteHover.hovered
                ToolTip.text: systemTrayManager.muted ? "取消静音" : "静音"
            }
            Slider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: systemTrayManager.volume; onMoved: systemTrayManager.setVolume(Math.round(value)) }
            Text { Layout.preferredWidth: 34; horizontalAlignment: Text.AlignRight; text: systemTrayManager.volume; color: themeManager.textSecondary; font.pixelSize: 10 }
        }

        RowLayout {
            Layout.fillWidth: true; spacing: 7
            Text { text: systemTrayManager.online ? "●" : "○"; color: systemTrayManager.online ? themeManager.success : themeManager.danger; font.pixelSize: 9 }
            Text { Layout.fillWidth: true; text: systemTrayManager.online ? "已连接网络" : "网络未连接"; color: themeManager.textMuted; font.pixelSize: 9 }
            Text { text: systemTrayManager.audioDeviceName; color: themeManager.textFaint; font.pixelSize: 8; elide: Text.ElideRight; Layout.maximumWidth: 170 }
        }

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            QuickTile { Layout.fillWidth: true; symbol: "♪"; label: "声音"; onClicked: root.openPanel(0) }
            QuickTile { Layout.fillWidth: true; symbol: "◎"; label: "网络"; onClicked: root.openPanel(1) }
            QuickTile { Layout.fillWidth: true; symbol: "◐"; label: "外观"; onClicked: root.openPanel(2) }
            QuickTile { Layout.fillWidth: true; symbol: "▣"; label: "侧栏"; onClicked: root.openPanel(3) }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: systemTrayManager.batteryPresent
            Text { text: systemTrayManager.charging ? "⚡" : "▰"; color: systemTrayManager.charging ? themeManager.success : themeManager.textSecondary; font.pixelSize: 11 }
            Text { Layout.fillWidth: true; text: systemTrayManager.charging ? "电池正在充电" : "电池电量"; color: themeManager.textMuted; font.pixelSize: 10 }
            Text { text: systemTrayManager.batteryPercent + "%"; color: themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold }
        }
    }

    Popup {
        id: controlPopup
        x: 0
        y: -190
        width: root.width
        height: 390
        padding: 14
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onVisibleChanged: root.popupVisibilityChanged(visible)
        background: Rectangle { radius: 22; color: themeManager.background; border.width: 1; border.color: themeManager.borderStrong }

        contentItem: ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: ["声音控制", "网络状态", "界面外观", "侧栏行为"][root.currentPanel]; color: themeManager.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
                Rectangle {
                    Layout.preferredWidth: 30; Layout.preferredHeight: 30; radius: 10
                    color: closeHover.hovered ? themeManager.surfaceHover : themeManager.surface
                    Text { anchors.centerIn: parent; text: "×"; color: themeManager.textSecondary; font.pixelSize: 15 }
                    HoverHandler { id: closeHover }
                    TapHandler { onTapped: controlPopup.close() }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: themeManager.border }

            StackLayout {
                Layout.fillWidth: true; Layout.fillHeight: true
                currentIndex: root.currentPanel

                ColumnLayout {
                    spacing: 14
                    Text { text: systemTrayManager.audioDeviceName || "默认播放设备"; color: themeManager.textSecondary; font.pixelSize: 11 }
                    Text { text: systemTrayManager.muted ? "当前已静音" : "主音量 " + systemTrayManager.volume + "%"; color: systemTrayManager.muted ? themeManager.danger : themeManager.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Slider { Layout.fillWidth: true; from: 0; to: 100; value: systemTrayManager.volume; onMoved: systemTrayManager.setVolume(Math.round(value)) }
                    ControlButton { Layout.fillWidth: true; label: systemTrayManager.muted ? "取消静音" : "静音"; active: systemTrayManager.muted; onClicked: systemTrayManager.toggleMuted() }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    spacing: 12
                    StatusCard { Layout.fillWidth: true; title: systemTrayManager.online ? "网络已连接" : "网络未连接"; detail: systemTrayManager.online ? "系统当前可以访问网络" : "请检查网络适配器或连接"; healthy: systemTrayManager.online }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        MetricCard { Layout.fillWidth: true; title: "下载"; value: networkMonitor.downloadSpeedText }
                        MetricCard { Layout.fillWidth: true; title: "上传"; value: networkMonitor.uploadSpeedText }
                    }
                    Text { Layout.fillWidth: true; text: "这里保持只读，不会误断开当前连接。后续可接入 Wi‑Fi 扫描与网络切换。"; wrapMode: Text.WordWrap; color: themeManager.textFaint; font.pixelSize: 9 }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    spacing: 12
                    Text { text: "主题模式"; color: themeManager.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 7
                        ChoiceButton { Layout.fillWidth: true; label: "跟随系统"; selected: themeManager.mode === 0; onClicked: themeManager.setModeValue(0) }
                        ChoiceButton { Layout.fillWidth: true; label: "浅色"; selected: themeManager.mode === 1; onClicked: themeManager.setModeValue(1) }
                        ChoiceButton { Layout.fillWidth: true; label: "深色"; selected: themeManager.mode === 2; onClicked: themeManager.setModeValue(2) }
                    }
                    Text { text: "面板透明度  " + Math.round(uiSettings.panelOpacity * 100) + "%"; color: themeManager.textSecondary; font.pixelSize: 11 }
                    Slider { Layout.fillWidth: true; from: 0.72; to: 1.0; stepSize: 0.01; value: uiSettings.panelOpacity; onMoved: uiSettings.panelOpacity = value }
                    ControlButton { Layout.fillWidth: true; label: "紧凑布局"; active: uiSettings.compactMode; onClicked: uiSettings.compactMode = !uiSettings.compactMode }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    spacing: 12
                    Text { text: "面板宽度  " + uiSettings.panelWidth + " px"; color: themeManager.textSecondary; font.pixelSize: 11 }
                    Slider { Layout.fillWidth: true; from: 360; to: 560; stepSize: 10; value: uiSettings.panelWidth; onMoved: uiSettings.panelWidth = Math.round(value) }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        ChoiceButton { Layout.fillWidth: true; label: "停靠左侧"; selected: displayManager.dockSide === 0; onClicked: displayManager.setDockSide(0) }
                        ChoiceButton { Layout.fillWidth: true; label: "停靠右侧"; selected: displayManager.dockSide === 1; onClicked: displayManager.setDockSide(1) }
                    }
                    ControlButton { Layout.fillWidth: true; label: "自动隐藏"; active: uiSettings.autoHide; onClicked: uiSettings.autoHide = !uiSettings.autoHide }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    component QuickTile: Rectangle {
        id: tile
        property string symbol
        property string label
        signal clicked()
        implicitHeight: 48; radius: 12
        color: tileHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
        border.width: 1; border.color: tileHover.hovered ? themeManager.accent : themeManager.border
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: tile.symbol; color: themeManager.accent; font.pixelSize: 12 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: tile.label; color: themeManager.textSecondary; font.pixelSize: 8 }
        }
        HoverHandler { id: tileHover }
        TapHandler { onTapped: tile.clicked() }
    }

    component ControlButton: Rectangle {
        id: control
        property string label
        property bool active: false
        signal clicked()
        implicitHeight: 42; radius: 13
        color: active ? themeManager.accentSoft : controlHover.hovered ? themeManager.surfaceHover : themeManager.surface
        border.width: 1; border.color: active ? themeManager.accent : themeManager.border
        Text { anchors.centerIn: parent; text: control.label; color: control.active ? themeManager.accent : themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold }
        HoverHandler { id: controlHover }
        TapHandler { onTapped: control.clicked() }
    }

    component ChoiceButton: ControlButton { property bool selected: false; active: selected }

    component StatusCard: Rectangle {
        id: statusCard
        property string title
        property string detail
        property bool healthy
        implicitHeight: 76; radius: 15; color: themeManager.surface; border.width: 1; border.color: healthy ? themeManager.success : themeManager.danger
        Column {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text { text: statusCard.title; color: themeManager.textPrimary; font.pixelSize: 12; font.weight: Font.DemiBold }
            Text { text: statusCard.detail; color: themeManager.textMuted; font.pixelSize: 9 }
        }
    }

    component MetricCard: Rectangle {
        id: metricCard
        property string title
        property string value
        implicitHeight: 72; radius: 14; color: themeManager.surface; border.width: 1; border.color: themeManager.border
        Column {
            anchors.centerIn: parent
            spacing: 4
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: metricCard.value; color: themeManager.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: metricCard.title; color: themeManager.textMuted; font.pixelSize: 9 }
        }
    }
}
