import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

CardSurface {
    id: root
    property bool expanded: false
    property string pending: ""
    property bool pendingActive: false
    implicitHeight: content.implicitHeight + 32
    onVisibleChanged: if (!visible) { pending = ""; password.text = "" }
    ColumnLayout {
        id: content
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "移动热点"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
            CardAction { text: root.expanded ? "收起" : "配置"; onClicked: { root.expanded = !root.expanded; root.pending = ""; password.text = ""; name.text = hotspotManager.ssid } }
        }
        Text { Layout.fillWidth: true; text: hotspotManager.status; color: themeManager.textMuted; font.pixelSize: 11; wrapMode: Text.Wrap }
        Text { Layout.fillWidth: true; visible: hotspotManager.ssid.length > 0; text: "名称 · " + hotspotManager.ssid; color: themeManager.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        RowLayout {
            CardAction { text: hotspotManager.active ? "关闭热点" : "开启热点"; enabled: hotspotManager.supported && !hotspotManager.busy; onClicked: { root.pendingActive = !hotspotManager.active; root.pending = "toggle" } }
            CardAction { text: "刷新"; enabled: !hotspotManager.busy; onClicked: hotspotManager.refresh() }
        }
        CardAction { visible: !hotspotManager.supported; text: "Windows 热点设置"; onClicked: Qt.openUrlExternally("ms-settings:network-mobilehotspot") }
        ColumnLayout {
            Layout.fillWidth: true; visible: root.expanded; spacing: 8
            Basic.TextField { id: name; Layout.fillWidth: true; placeholderText: "热点名称（最多 32 字节）"; enabled: !hotspotManager.busy && !hotspotManager.active; color: themeManager.textPrimary; background: Rectangle { radius: 8; color: themeManager.surfaceStrong } }
            Basic.TextField { id: password; Layout.fillWidth: true; placeholderText: "新密码（8–63 位英文字符）"; echoMode: TextInput.Password; maximumLength: 63; enabled: !hotspotManager.busy && !hotspotManager.active; color: themeManager.textPrimary; background: Rectangle { radius: 8; color: themeManager.surfaceStrong } }
            CardAction { text: "保存配置"; enabled: hotspotManager.supported && !hotspotManager.active && !hotspotManager.busy; onClicked: root.pending = "configure" }
            Text { Layout.fillWidth: true; text: "使用 Windows 当前默认连接共享。请先关闭热点再修改配置；密码不保存到应用，不记录日志。"; wrapMode: Text.Wrap; color: themeManager.textFaint; font.pixelSize: 10 }
        }
        ColumnLayout {
            visible: root.pending.length > 0; Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: root.pending === "configure" ? "确认修改 Windows 热点名称和密码？" : root.pendingActive ? "确认开启网络共享？可能产生流量费用。" : "确认关闭热点？已连接设备将断开。"; color: themeManager.warning; wrapMode: Text.Wrap; font.pixelSize: 11 }
            RowLayout {
                CardAction { text: "确认"; enabled: !hotspotManager.busy; onClicked: { if (root.pending === "configure") { hotspotManager.configure(name.text, password.text); password.text = "" } else hotspotManager.setActive(root.pendingActive); root.pending = "" } }
                CardAction { text: "取消"; onClicked: { root.pending = ""; password.text = "" } }
            }
        }
    }
}
