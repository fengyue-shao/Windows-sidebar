import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property int currentIndex: 0
    property bool mediaEnabled: true
    property bool monitorEnabled: true
    property bool notificationsEnabled: true
    property var pluginPages: []
    signal sectionRequested(int index)
    signal mediaEnabledRequested(bool enabled)
    signal monitorEnabledRequested(bool enabled)
    signal notificationsEnabledRequested(bool enabled)
    signal popupVisibilityChanged(bool visible)
    signal exitRequested()
    function closeSettings() { settingsPopup.close() }
    function openSettings() { settingsPopup.open() }

    implicitHeight: 54
    radius: 16
    color: themeManager.cardSurface
    border.width: 1
    border.color: themeManager.borderStrong

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4
        SectionButton { Layout.fillWidth: true; label: "监控"; iconName: "desktop"; selected: root.currentIndex === 0; onClicked: root.sectionRequested(0) }
        SectionButton { Layout.fillWidth: true; label: "通知"; iconName: "alert"; selected: root.currentIndex === 1; onClicked: root.sectionRequested(1) }
        SectionButton { Layout.fillWidth: true; label: "专注"; iconName: "timer"; selected: root.currentIndex === 2; onClicked: root.sectionRequested(2) }
        SectionButton { Layout.fillWidth: true; label: "剪贴"; iconName: "clipboard"; selected: root.currentIndex === 3; onClicked: root.sectionRequested(3) }
        SectionButton { Layout.fillWidth: true; label: "工具箱"; iconName: "toolbox"; selected: root.currentIndex === 4; onClicked: root.sectionRequested(4) }
        Repeater {
            model: root.pluginPages
            SectionButton {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                label: modelData.title
                symbol: modelData.symbol
                selected: root.currentIndex === 5 + index
                onClicked: root.sectionRequested(5 + index)
            }
        }
        Rectangle { visible: false; Layout.preferredWidth: 1; Layout.preferredHeight: 26; color: themeManager.track }
        Rectangle {
            visible: false
            Layout.preferredWidth: 40
            Layout.fillHeight: true
            radius: 13
            color: settingsTap.pressed ? themeManager.accentSoft : settingsHover.hovered ? themeManager.surfaceHover : themeManager.overlayStart
            FluentIcon { anchors.centerIn: parent; name: "settings"; tint: settingsHover.hovered ? themeManager.textPrimary : themeManager.textSecondary }
            HoverHandler { id: settingsHover }
            TapHandler { id: settingsTap; onTapped: settingsPopup.open() }
            ToolTip.visible: settingsHover.hovered
            ToolTip.text: "设置"
        }
    }

    Popup {
        id: settingsPopup
        parent: root.Window.window ? root.Window.window.contentItem : root
        onVisibleChanged: root.popupVisibilityChanged(visible)
        x: root.width - width
        y: 72
        width: root.width
        height: Math.max(100, Math.min(620, root.Window.window ? root.Window.window.height - 80 : 620))
        modal: true
        dim: false
        padding: 12
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 22; color: themeManager.cardSurface; border.width: 1; border.color: themeManager.borderStrong }
        contentItem: Flickable {
            contentWidth: width
            contentHeight: settingsColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: settingsColumn
                width: parent.width
                spacing: 10
                Text { text: "设置"; color: themeManager.textPrimary; font.pixelSize: 20; font.weight: Font.DemiBold }
                Text { text: "所有更改即时生效，无需重启应用"; color: themeManager.textMuted; font.pixelSize: 10 }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: themeManager.border }

                SectionLabel { text: "界面模式" }
                SettingsRow {
                    Layout.fillWidth: true
                    title: "跟随系统"
                    detail: "自动响应 Windows 颜色模式"
                    selected: themeManager.mode === 0
                    badge: "推荐"
                    onClicked: themeManager.setModeValue(0)
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    ModeCard { Layout.fillWidth: true; title: "白天模式"; lightPreview: true; selected: themeManager.mode === 1; onClicked: themeManager.setModeValue(1) }
                    ModeCard { Layout.fillWidth: true; title: "黑夜模式"; selected: themeManager.mode === 2; onClicked: themeManager.setModeValue(2) }
                }

                SectionLabel { text: "显示位置" }
                SettingsRow {
                    Layout.fillWidth: true
                    readonly property var display: displayManager.displays.length > 0 ? displayManager.displays[displayManager.currentIndex] : null
                    title: display ? display.label : "显示器"
                    detail: displayManager.displays.length > 1 ? "点击切换显示器" : "当前唯一显示器"
                    trailing: "⌄"
                    onClicked: if (displayManager.displays.length > 1) displayManager.setCurrentIndex((displayManager.currentIndex + 1) % displayManager.displays.length)
                }
                Text { text: "顶部居中 · 在屏幕顶部停留可唤出精简栏"; color: themeManager.textMuted; font.pixelSize: 11 }

                SectionLabel { text: "面板风格" }
                ModuleRow { Layout.fillWidth: true; title: "离开后自动隐藏"; enabledValue: uiSettings.autoHide; onClicked: uiSettings.autoHide = !uiSettings.autoHide }
                ModuleRow { Layout.fillWidth: true; title: "紧凑布局"; enabledValue: uiSettings.compactMode; onClicked: uiSettings.compactMode = !uiSettings.compactMode }
                ValueSlider {
                    Layout.fillWidth: true
                    title: "卡片栏宽度"
                    valueText: Math.round(uiSettings.panelWidth * 1.8) + " px"
                    from: 360; to: 560; stepSize: 10; value: uiSettings.panelWidth
                    onMoved: uiSettings.panelWidth = Math.round(value)
                }
                ValueSlider {
                    Layout.fillWidth: true
                    title: "透明度"
                    valueText: Math.round(uiSettings.panelOpacity * 100) + "%"
                    from: 0.94; to: 1.0; stepSize: 0.01; value: uiSettings.panelOpacity
                    onMoved: uiSettings.panelOpacity = value
                }

                SectionLabel { text: "信息模块" }
                ModuleRow { Layout.fillWidth: true; title: "媒体播放器"; enabledValue: root.mediaEnabled; onClicked: root.mediaEnabledRequested(!root.mediaEnabled) }
                ModuleRow { Layout.fillWidth: true; title: "通知中心"; enabledValue: root.notificationsEnabled; onClicked: root.notificationsEnabledRequested(!root.notificationsEnabled) }

                Text {
                    Layout.fillWidth: true
                    text: dropdownController.hotkeyAvailable
                        ? "全局 Ctrl+Alt+Space 唤出/隐藏 · Ctrl+1–5 切页 · Esc 收起 · Ctrl+Q 退出"
                        : "全局快捷键被占用，请使用顶部边缘唤出 · 应用内 Ctrl+Space 唤出/隐藏"
                    wrapMode: Text.WordWrap
                    color: themeManager.textFaint
                    font.pixelSize: 9
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: themeManager.border }
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 36; radius: 12
                    color: resetHover.hovered ? themeManager.surfaceHover : themeManager.overlayStart
                    Text { anchors.centerIn: parent; text: "恢复默认布局"; color: themeManager.textSecondary; font.pixelSize: 10 }
                    HoverHandler { id: resetHover }
                    TapHandler { onTapped: uiSettings.reset() }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 40; radius: 12
                    color: exitHover.hovered ? themeManager.dangerSoft : themeManager.overlayStart
                    Text { anchors.centerIn: parent; text: "退出 Windows Sidebar"; color: themeManager.danger; font.pixelSize: 11; font.weight: Font.DemiBold }
                    HoverHandler { id: exitHover }
                    TapHandler { onTapped: root.exitRequested() }
                }
            }
        }
    }

    component SectionLabel: Text {
        Layout.topMargin: 4
        color: themeManager.textPrimary
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }

    component SettingsRow: Rectangle {
        id: row
        property string title
        property string detail
        property string trailing: ""
        property string badge: ""
        property bool selected: false
        signal clicked()
        implicitHeight: 58
        radius: 15
        color: selected ? themeManager.accentSoft : rowHover.hovered ? themeManager.surfaceHover : themeManager.surface
        border.width: 1
        border.color: selected ? themeManager.accent : themeManager.border
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 13; anchors.rightMargin: 13; spacing: 10
            Rectangle {
                Layout.preferredWidth: 22; Layout.preferredHeight: 22; radius: 11
                color: row.selected ? themeManager.accentSoft : themeManager.track
                border.width: 1; border.color: row.selected ? themeManager.accent : themeManager.borderStrong
                Text { anchors.centerIn: parent; text: row.selected ? "✓" : ""; color: themeManager.accent; font.pixelSize: 12 }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 1
                Text { Layout.fillWidth: true; text: row.title; color: themeManager.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                Text { Layout.fillWidth: true; text: row.detail; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
            }
            Text { visible: row.badge.length > 0; text: row.badge; color: themeManager.accent; font.pixelSize: 9 }
            Text { visible: row.trailing.length > 0; text: row.trailing; color: themeManager.textMuted; font.pixelSize: 12 }
        }
        HoverHandler { id: rowHover }
        TapHandler { onTapped: row.clicked() }
    }

    component ModeCard: Rectangle {
        id: card
        property string title
        property bool lightPreview: false
        property bool selected: false
        signal clicked()
        implicitHeight: 72
        radius: 14
        color: lightPreview ? "#F7F9FC" : "#242832"
        border.width: selected ? 2 : 1
        border.color: selected ? themeManager.accent : themeManager.borderStrong
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 10; spacing: 7
            RowLayout {
                Layout.fillWidth: true
                Rectangle { Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5; color: card.lightPreview ? "#FDBA55" : "#9B8CFF" }
                Text { Layout.fillWidth: true; text: card.title; color: card.lightPreview ? "#1C2430" : "#F2F5FA"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Text { visible: card.selected; text: "✓"; color: themeManager.accent; font.pixelSize: 10 }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 20; radius: 7; color: card.lightPreview ? "#E9EDF3" : "#171A21"
                Rectangle { anchors.left: parent.left; anchors.leftMargin: 7; anchors.verticalCenter: parent.verticalCenter; width: parent.width * .46; height: 7; radius: 4; color: card.lightPreview ? "#FFFFFF" : "#303541" }
            }
        }
        TapHandler { onTapped: card.clicked() }
    }

    component DockButton: Rectangle {
        id: dock
        property string text
        property bool selected: false
        signal clicked()
        implicitHeight: 40
        radius: 12
        color: selected ? themeManager.accentSoft : themeManager.surface
        border.width: 1
        border.color: selected ? themeManager.accent : themeManager.border
        Text { anchors.centerIn: parent; text: dock.text; color: dock.selected ? themeManager.accent : themeManager.textMuted; font.pixelSize: 10; font.weight: dock.selected ? Font.DemiBold : Font.Normal }
        TapHandler { onTapped: dock.clicked() }
    }

    component ModuleRow: Rectangle {
        id: moduleRow
        property string title
        property bool enabledValue: true
        signal clicked()
        implicitHeight: 42
        radius: 12
        color: themeManager.surface
        border.width: 1
        border.color: themeManager.border
        Text { anchors.left: parent.left; anchors.leftMargin: 13; anchors.verticalCenter: parent.verticalCenter; text: moduleRow.title; color: themeManager.textPrimary; font.pixelSize: 10 }
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
            width: 40; height: 22; radius: 11; color: moduleRow.enabledValue ? themeManager.accent : themeManager.track
            Rectangle { x: moduleRow.enabledValue ? parent.width - width - 3 : 3; anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; radius: 8; color: "#FFFFFF"; Behavior on x { NumberAnimation { duration: 140 } } }
        }
        TapHandler { onTapped: moduleRow.clicked() }
    }

    component ValueSlider: ColumnLayout {
        id: valueSlider
        property string title
        property string valueText
        property alias from: slider.from
        property alias to: slider.to
        property alias stepSize: slider.stepSize
        property alias value: slider.value
        signal moved()
        spacing: 3
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: valueSlider.title; color: themeManager.textPrimary; font.pixelSize: 10 }
            Text { text: valueSlider.valueText; color: themeManager.textMuted; font.pixelSize: 9 }
        }
        Slider {
            id: slider
            Layout.fillWidth: true
            onMoved: valueSlider.moved()
        }
    }

    component SectionButton: Rectangle {
        id: button
        property string label
        property string symbol
        property string iconName: ""
        property bool selected: false
        signal clicked()
        Layout.fillHeight: true
        radius: 12
        color: selected ? themeManager.accentSoft : buttonHover.hovered ? themeManager.surfaceHover : themeManager.overlayStart
        Row {
            anchors.centerIn: parent; spacing: 6
            FluentIcon { visible: button.iconName.length > 0; name: button.iconName; width: 16; height: 16; tint: button.selected ? themeManager.accent : themeManager.textMuted }
            Text { visible: button.iconName.length === 0; text: button.symbol; color: button.selected ? themeManager.accent : themeManager.textMuted; font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }
            Text { text: button.label; color: button.selected ? themeManager.textPrimary : themeManager.textMuted; font.pixelSize: 9; font.weight: button.selected ? Font.DemiBold : Font.Normal; anchors.verticalCenter: parent.verticalCenter }
        }
        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 2; width: button.selected ? 20 : 0; height: 2; radius: 1; color: themeManager.accent; Behavior on width { NumberAnimation { duration: 140 } } }
        HoverHandler { id: buttonHover }
        TapHandler { onTapped: button.clicked() }
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
