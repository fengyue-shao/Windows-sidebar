import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

CardSurface {
    id: root
    implicitHeight: content.implicitHeight + 32
    AudioLevelBar {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.rightMargin: 20; anchors.bottomMargin: 35
        width: Math.min(parent.width - 40, 300); height: 100
        opacity: 0.10
        level: audioInputManager.inputPeak
        available: audioInputManager.inputMeterAvailable; muted: audioInputManager.muted
    }
    ColumnLayout {
        id: content
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
        spacing: 10
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            FluentIcon { name: audioInputManager.muted ? "mic_off" : "mic"; tint: audioInputManager.muted ? themeManager.danger : themeManager.accent }
            Text { Layout.fillWidth: true; text: "麦克风"; color: themeManager.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
            CardAction { text: audioInputManager.muted ? "取消静音" : "静音"; enabled: audioInputManager.available; onClicked: audioInputManager.toggleMuted() }
        }
        Text { Layout.fillWidth: true; text: audioInputManager.deviceName || "没有输入设备"; color: themeManager.textSecondary; elide: Text.ElideRight; font.pixelSize: 11 }
        Text { Layout.fillWidth: true; text: captureUsageMonitor.applications.length ? "正在使用："+captureUsageMonitor.applications.map(app=>app.name).join(" · ") : "未检测到标准活动输入会话"; color: captureUsageMonitor.applications.length ? themeManager.accent : themeManager.textMuted; font.pixelSize: 10; wrapMode: Text.Wrap }
        Text { Layout.fillWidth: true; text: captureUsageMonitor.statusText; color: themeManager.textFaint; font.pixelSize: 9; wrapMode: Text.Wrap }
        RowLayout {
            Layout.fillWidth: true
            CardSlider { Layout.fillWidth: true; enabled: audioInputManager.available; from: 0; to: 100; stepSize: 1; value: audioInputManager.volume; onMoved: audioInputManager.setVolume(Math.round(value)) }
            Text { text: audioInputManager.volume + "%"; color: themeManager.textMuted; font.pixelSize: 10 }
        }
        Text { Layout.fillWidth: true; text: !audioInputManager.inputMeterAvailable ? "输入电平不可用" : audioInputManager.muted ? "默认麦克风已静音" : "实时输入峰值 · 不开启录音"; color: themeManager.textMuted; font.pixelSize: 10 }
        CardAction { Layout.fillWidth: true; text: "切换设备 / Windows 输入设置"; onClicked: audioInputManager.openInputSettings() }
        Text { Layout.fillWidth: true; text: audioInputManager.statusText; color: themeManager.textFaint; font.pixelSize: 9; wrapMode: Text.Wrap }
    }
}
