import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property int currentPanel: 0
    property bool expanded: false
    property string selectedWifiSsid: ""
    property string selectedWifiProfile: ""
    property bool selectedWifiSecure: false
    readonly property int summaryHeight: systemTrayManager.batteryPresent ? 188 : 164
    readonly property int detailHeight: (currentPanel === 0 || currentPanel === 1 || currentPanel === 4) ? 430 : 286

    function selectPanel(index) {
        if (expanded && currentPanel === index) {
            expanded = false
            return
        }
        currentPanel = index
        expanded = true
        if (index === 4 && bluetoothManager.devices.length === 0)
            bluetoothManager.scan()
    }

    implicitHeight: summaryHeight + (expanded ? detailHeight + 20 : 0)
    radius: 16
    color: themeManager.cardSurface
    border.width: 1
    border.color: expanded ? themeManager.borderStrong : themeManager.border
    clip: true

    Behavior on implicitHeight { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: 160 } }

    AudioLevelBar {
        anchors.right: parent.right; anchors.rightMargin: 20
        y: 15; width: Math.min(parent.width - 40, 300); height: 92
        opacity: 0.10
        level: audioInputManager.outputPeak
        available: audioInputManager.outputMeterAvailable; muted: systemTrayManager.muted
    }

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
            }
            CardSlider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: systemTrayManager.volume; onMoved: systemTrayManager.setVolume(Math.round(value)) }
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
            QuickTile { Layout.fillWidth: true; symbol: "♪"; label: "声音"; selected: root.expanded && root.currentPanel === 0; onClicked: root.selectPanel(0) }
            QuickTile { Layout.fillWidth: true; symbol: "◎"; label: "网络"; selected: root.expanded && root.currentPanel === 1; onClicked: root.selectPanel(1) }
            QuickTile { Layout.fillWidth: true; symbol: "ᛒ"; label: "蓝牙"; selected: root.expanded && root.currentPanel === 4; onClicked: root.selectPanel(4) }
            QuickTile { Layout.fillWidth: true; symbol: "◐"; label: "外观"; selected: root.expanded && root.currentPanel === 2; onClicked: root.selectPanel(2) }
            QuickTile { Layout.fillWidth: true; symbol: "▣"; label: "布局"; selected: root.expanded && root.currentPanel === 3; onClicked: root.selectPanel(3) }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: systemTrayManager.batteryPresent
            Text { text: systemTrayManager.charging ? "⚡" : "▰"; color: systemTrayManager.charging ? themeManager.success : themeManager.textSecondary; font.pixelSize: 11 }
            Text { Layout.fillWidth: true; text: systemTrayManager.charging ? "电池正在充电" : "电池电量"; color: themeManager.textMuted; font.pixelSize: 10 }
            Text { text: systemTrayManager.batteryPercent + "%"; color: themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.expanded ? 1 : 0
            visible: root.expanded
            color: themeManager.border
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.expanded ? root.detailHeight : 0
            visible: root.expanded
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: ["声音控制", "Wi-Fi 网络", "界面外观", "下拉栏行为", "蓝牙设备"][root.currentPanel]; color: themeManager.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Rectangle {
                        Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 9
                        color: collapseHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
                        Text { anchors.centerIn: parent; text: "⌃"; color: themeManager.textSecondary; font.pixelSize: 12 }
                        HoverHandler { id: collapseHover }
                        TapHandler { onTapped: root.expanded = false }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    currentIndex: root.currentPanel

                    ColumnLayout {
                        spacing: 10
                        Text { text: "输出设备"; color: themeManager.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold }
                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            orientation: ListView.Horizontal
                            spacing: 6
                            clip: true
                            model: systemTrayManager.audioOutputs
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Rectangle {
                                required property var modelData
                                height: 38
                                width: Math.min(190, Math.max(104, outputName.implicitWidth + 38))
                                radius: 11
                                color: modelData.selected ? themeManager.accentSoft : outputHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
                                border.width: 1
                                border.color: modelData.selected ? themeManager.accent : themeManager.border
                                Row {
                                    anchors.centerIn: parent; spacing: 6
                                    Text { text: modelData.selected ? "●" : "○"; color: modelData.selected ? themeManager.accent : themeManager.textFaint; font.pixelSize: 8 }
                                    Text { id: outputName; text: modelData.name; color: modelData.selected ? themeManager.accent : themeManager.textPrimary; font.pixelSize: 9; elide: Text.ElideRight; width: Math.min(145, implicitWidth) }
                                }
                                HoverHandler { id: outputHover }
                                TapHandler { onTapped: systemTrayManager.selectAudioOutput(modelData.id) }
                            }
                        }
                        Text { text: systemTrayManager.muted ? "当前已静音" : "主音量 " + systemTrayManager.volume + "%"; color: systemTrayManager.muted ? themeManager.danger : themeManager.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold }
                        Slider { Layout.fillWidth: true; from: 0; to: 100; value: systemTrayManager.volume; onMoved: systemTrayManager.setVolume(Math.round(value)) }
                        ControlButton { Layout.fillWidth: true; label: systemTrayManager.muted ? "取消静音" : "静音"; active: systemTrayManager.muted; onClicked: systemTrayManager.toggleMuted() }
                        Text { text: "应用音量"; color: themeManager.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 5
                            model: systemTrayManager.audioSessions
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 54
                                radius: 12
                                color: themeManager.surfaceStrong
                                border.width: 1
                                border.color: modelData.active ? themeManager.accentSoft : themeManager.border
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 7
                                    Rectangle {
                                        Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 9
                                        color: modelData.muted ? themeManager.dangerSoft : themeManager.accentSoft
                                        Text { anchors.centerIn: parent; text: modelData.muted ? "×" : "♪"; color: modelData.muted ? themeManager.danger : themeManager.accent; font.pixelSize: 11 }
                                        TapHandler { onTapped: systemTrayManager.toggleSessionMuted(modelData.id) }
                                    }
                                    ColumnLayout {
                                        Layout.preferredWidth: 82
                                        spacing: 1
                                        Text { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight; color: themeManager.textPrimary; font.pixelSize: 9; font.weight: Font.DemiBold }
                                        Text { text: modelData.active ? "正在播放" : "已暂停"; color: modelData.active ? themeManager.success : themeManager.textFaint; font.pixelSize: 8 }
                                    }
                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0; to: 100; stepSize: 1
                                        value: modelData.volume
                                        onMoved: systemTrayManager.setSessionVolume(modelData.id, Math.round(value))
                                    }
                                    Text { Layout.preferredWidth: 25; text: modelData.volume; horizontalAlignment: Text.AlignRight; color: themeManager.textMuted; font.pixelSize: 8 }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: parent.count === 0
                                text: "暂无应用音频会话"
                                color: themeManager.textFaint
                                font.pixelSize: 9
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: wifiManager.statusText; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
                            RadioToggle { label: "Wi-Fi"; checked: radioManager.wifiEnabled; onToggled: radioManager.setWifiEnabled(checked) }
                            Rectangle {
                                Layout.preferredWidth: 28; Layout.preferredHeight: 26; radius: 8
                                color: wifiRefreshHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
                                Text { anchors.centerIn: parent; text: wifiManager.scanning ? "…" : "↻"; color: themeManager.accent; font.pixelSize: 11 }
                                HoverHandler { id: wifiRefreshHover }
                                TapHandler { enabled: !wifiManager.scanning; onTapped: wifiManager.scan() }
                                ToolTip.visible: wifiRefreshHover.hovered
                                ToolTip.text: "扫描附近 Wi-Fi"
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            MetricCard { Layout.fillWidth: true; title: "下载"; value: networkMonitor.downloadSpeedText }
                            MetricCard { Layout.fillWidth: true; title: "上传"; value: networkMonitor.uploadSpeedText }
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 5
                            model: wifiManager.networks
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 48
                                radius: 11
                                color: modelData.connected ? themeManager.accentSoft : wifiItemHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
                                border.width: 1
                                border.color: modelData.connected ? themeManager.accent : themeManager.border
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 8
                                    Text { text: modelData.strength >= 75 ? "▰" : modelData.strength >= 45 ? "▱" : "·"; color: modelData.connected ? themeManager.accent : themeManager.textSecondary; font.pixelSize: 12 }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 1
                                        Text { Layout.fillWidth: true; text: modelData.ssid; color: themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                        Text { text: modelData.connected ? "已连接" : (modelData.secure ? "安全网络 · " : "开放网络 · ") + modelData.strength + "%"; color: modelData.connected ? themeManager.success : themeManager.textFaint; font.pixelSize: 8 }
                                    }
                                    Text { text: modelData.secure ? "⌾" : ""; color: themeManager.textMuted; font.pixelSize: 10 }
                                }
                                HoverHandler { id: wifiItemHover }
                                TapHandler {
                                    enabled: !modelData.connected
                                    onTapped: {
                                        root.selectedWifiSsid = modelData.ssid
                                        root.selectedWifiProfile = modelData.profileName
                                        root.selectedWifiSecure = modelData.secure
                                        wifiPassword.text = ""
                                        if (modelData.profileName !== "" || !modelData.secure)
                                            wifiManager.connectNetwork(modelData.ssid, modelData.profileName, "", modelData.secure)
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: root.selectedWifiSecure && root.selectedWifiProfile === ""
                            TextField {
                                id: wifiPassword
                                Layout.fillWidth: true
                                placeholderText: root.selectedWifiSsid ? "输入 " + root.selectedWifiSsid + " 的密码" : "选择需要密码的网络"
                                echoMode: TextInput.Password
                                color: themeManager.textPrimary
                                enabled: root.selectedWifiSsid !== ""
                                onAccepted: wifiConnectButton.clicked()
                            }
                            ControlButton {
                                id: wifiConnectButton
                                Layout.preferredWidth: 64
                                label: "连接"
                                enabled: wifiPassword.text.length >= 8
                                onClicked: wifiManager.connectNetwork(root.selectedWifiSsid, "", wifiPassword.text, true)
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            ChoiceButton { Layout.fillWidth: true; label: "跟随系统"; selected: themeManager.mode === 0; onClicked: themeManager.setModeValue(0) }
                            ChoiceButton { Layout.fillWidth: true; label: "浅色"; selected: themeManager.mode === 1; onClicked: themeManager.setModeValue(1) }
                            ChoiceButton { Layout.fillWidth: true; label: "深色"; selected: themeManager.mode === 2; onClicked: themeManager.setModeValue(2) }
                        }
                        Text { text: "面板透明度  " + Math.round(uiSettings.panelOpacity * 100) + "%"; color: themeManager.textSecondary; font.pixelSize: 10 }
                        Slider { Layout.fillWidth: true; from: 0.94; to: 1.0; stepSize: 0.01; value: uiSettings.panelOpacity; onMoved: uiSettings.panelOpacity = value }
                        ControlButton { Layout.fillWidth: true; label: "紧凑布局"; active: uiSettings.compactMode; onClicked: uiSettings.compactMode = !uiSettings.compactMode }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 10
                        Text { text: "卡片栏宽度  " + Math.round(uiSettings.panelWidth * 1.8) + " px"; color: themeManager.textSecondary; font.pixelSize: 10 }
                        Slider { Layout.fillWidth: true; from: 360; to: 560; stepSize: 10; value: uiSettings.panelWidth; onMoved: uiSettings.panelWidth = Math.round(value) }
                        Text { Layout.fillWidth: true; text: "顶部居中 · 边缘唤出精简栏\n点击展开后才显示卡片"; color: themeManager.textMuted; font.pixelSize: 11 }
                        ControlButton { Layout.fillWidth: true; label: "自动隐藏"; active: uiSettings.autoHide; onClicked: uiSettings.autoHide = !uiSettings.autoHide }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: bluetoothManager.statusText; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
                            RadioToggle { label: "蓝牙"; checked: radioManager.bluetoothEnabled; onToggled: radioManager.setBluetoothEnabled(checked) }
                            Rectangle {
                                Layout.preferredWidth: 28; Layout.preferredHeight: 26; radius: 8
                                color: bluetoothRefreshHover.hovered ? themeManager.accentSoft : themeManager.surfaceStrong
                                Text { anchors.centerIn: parent; text: bluetoothManager.scanning ? "…" : "↻"; color: themeManager.accent; font.pixelSize: 11 }
                                HoverHandler { id: bluetoothRefreshHover }
                                TapHandler { enabled: !bluetoothManager.scanning; onTapped: bluetoothManager.scan() }
                                ToolTip.visible: bluetoothRefreshHover.hovered
                                ToolTip.text: "扫描蓝牙设备"
                            }
                        }
                        ListView {
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                            model: bluetoothManager.devices
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width; height: 58; radius: 12
                                color: modelData.connected ? themeManager.accentSoft : bluetoothHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
                                border.width: 1; border.color: modelData.connected ? themeManager.accent : themeManager.border
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 8; spacing: 8
                                    Rectangle {
                                        Layout.preferredWidth: 30; Layout.preferredHeight: 30; radius: 9
                                        color: modelData.connected ? themeManager.accent : themeManager.accentSoft
                                        Text { anchors.centerIn: parent; text: "ᛒ"; color: modelData.connected ? "white" : themeManager.accent; font.pixelSize: 12; font.weight: Font.Bold }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 2
                                        Text { Layout.fillWidth: true; text: modelData.name; color: themeManager.textPrimary; font.pixelSize: 10; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                        Text { text: modelData.connected ? "已连接" : modelData.paired ? "已配对" : "可配对"; color: modelData.connected ? themeManager.success : themeManager.textMuted; font.pixelSize: 8 }
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: modelData.paired ? 58 : 48; Layout.preferredHeight: 28; radius: 9
                                        color: bluetoothActionHover.hovered ? (modelData.paired ? themeManager.dangerSoft : themeManager.accentSoft) : themeManager.overlayStart
                                        Text { anchors.centerIn: parent; text: modelData.paired ? "取消配对" : "配对"; color: modelData.paired ? themeManager.danger : themeManager.accent; font.pixelSize: 8; font.weight: Font.DemiBold }
                                        HoverHandler { id: bluetoothActionHover }
                                        TapHandler { onTapped: modelData.paired ? bluetoothManager.unpairDevice(modelData.id) : bluetoothManager.pairDevice(modelData.id) }
                                    }
                                }
                                HoverHandler { id: bluetoothHover }
                            }
                            Text { anchors.centerIn: parent; visible: parent.count === 0 && !bluetoothManager.scanning; text: "暂无蓝牙设备"; color: themeManager.textFaint; font.pixelSize: 9 }
                        }
                        Text { Layout.fillWidth: true; text: "已配对设备的音频、输入等服务由 Windows 自动连接"; color: themeManager.textFaint; font.pixelSize: 8; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
    }

    component QuickTile: Rectangle {
        id: tile
        property string symbol
        property string label
        property bool selected: false
        signal clicked()
        implicitHeight: 48; radius: 12
        color: selected ? themeManager.accentSoft : tileHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
        border.width: 1; border.color: selected ? themeManager.accent : themeManager.border
        Column {
            anchors.centerIn: parent; spacing: 2
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: tile.symbol; color: tile.selected ? themeManager.accent : themeManager.textSecondary; font.pixelSize: 12 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: tile.label; color: tile.selected ? themeManager.accent : themeManager.textSecondary; font.pixelSize: 8 }
        }
        HoverHandler { id: tileHover }
        TapHandler { onTapped: tile.clicked() }
    }

    component ControlButton: Rectangle {
        id: control
        property string label
        property bool active: false
        signal clicked()
        implicitHeight: 40; radius: 12
        opacity: enabled ? 1 : 0.45
        color: active ? themeManager.accentSoft : controlTap.pressed ? themeManager.overlayStart : controlHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
        border.width: 1; border.color: active ? themeManager.accent : themeManager.border
        Text { anchors.centerIn: parent; text: control.label; color: control.active ? themeManager.accent : themeManager.textPrimary; font.pixelSize: 9; font.weight: Font.DemiBold }
        HoverHandler { id: controlHover }
        TapHandler { id: controlTap; enabled: control.enabled; onTapped: control.clicked() }
    }

    component RadioToggle: Rectangle {
        id: radioToggle
        property string label
        property bool checked: false
        signal toggled(bool checked)
        implicitWidth: 78; implicitHeight: 28; radius: 10
        color: checked ? themeManager.accentSoft : toggleHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
        border.width: 1; border.color: checked ? themeManager.accent : themeManager.border
        Row {
            anchors.centerIn: parent; spacing: 6
            Rectangle {
                width: 20; height: 12; radius: 6
                color: radioToggle.checked ? themeManager.accent : themeManager.track
                Rectangle { x: radioToggle.checked ? 10 : 2; anchors.verticalCenter: parent.verticalCenter; width: 8; height: 8; radius: 4; color: "#FFFFFF"; Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } } }
            }
            Text { text: radioToggle.label; color: radioToggle.checked ? themeManager.accent : themeManager.textMuted; font.pixelSize: 8; font.weight: Font.DemiBold }
        }
        HoverHandler { id: toggleHover }
        TapHandler { onTapped: radioToggle.toggled(!radioToggle.checked) }
    }

    component ChoiceButton: ControlButton { property bool selected: false; active: selected }

    component StatusCard: Rectangle {
        id: statusCard
        property string title
        property string detail
        property bool healthy
        implicitHeight: 68; radius: 14; color: themeManager.surfaceStrong; border.width: 1; border.color: healthy ? themeManager.success : themeManager.danger
        Column {
            anchors.left: parent.left; anchors.leftMargin: 13; anchors.verticalCenter: parent.verticalCenter; spacing: 3
            Text { text: statusCard.title; color: themeManager.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
            Text { text: statusCard.detail; color: themeManager.textMuted; font.pixelSize: 9 }
        }
    }

    component MetricCard: Rectangle {
        id: metricCard
        property string title
        property string value
        implicitHeight: 66; radius: 13; color: themeManager.surfaceStrong; border.width: 1; border.color: themeManager.border
        Column {
            anchors.centerIn: parent; spacing: 3
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: metricCard.value; color: themeManager.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: metricCard.title; color: themeManager.textMuted; font.pixelSize: 8 }
        }
    }
}
