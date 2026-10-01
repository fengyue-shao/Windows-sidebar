import QtQuick
import QtQuick.Layouts

CardSurface {
    id: root
    property bool expanded: false
    property var adapterItems: networkDetails.adapters
    property string connectionSummary: networkDetails.summary
    implicitHeight: content.implicitHeight + 32
    ColumnLayout {
        id: content
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "网络详情"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
            CardAction { text: root.expanded ? "收起" : "详情"; onClicked: { root.expanded = !root.expanded; if (root.expanded) networkDetails.refresh() } }
        }
        Text { Layout.fillWidth: true; text: root.connectionSummary; color: themeManager.textMuted; font.pixelSize: 11; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        ColumnLayout {
            visible: root.expanded; Layout.fillWidth: true; spacing: 10
            Repeater {
                model: root.adapterItems
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true; implicitHeight: details.implicitHeight + 24
                    radius: 10; color: themeManager.surface
                    ColumnLayout {
                        id: details
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12; spacing: 5
                        Text { Layout.fillWidth: true; text: modelData.name; color: themeManager.textPrimary; font.pixelSize: 12; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                        Text { Layout.fillWidth: true; text: modelData.description; color: themeManager.textMuted; font.pixelSize: 10; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                        Text { Layout.fillWidth: true; text: "IP  " + modelData.addresses + "\n网关  " + (modelData.gateways || "未配置") + "\nDNS  " + (modelData.dns || "未配置") + "\nMTU  " + modelData.mtu + " · " + modelData.speed; color: themeManager.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                    }
                }
            }
            Text { Layout.fillWidth: true; text: root.adapterItems.length === 0 ? "没有活动的非回环适配器" : "链路速率不是实时网速；连接状态不保证所有网站可访问。"; color: themeManager.textFaint; font.pixelSize: 10; wrapMode: Text.Wrap }
            CardAction { text: "刷新详情"; onClicked: networkDetails.refresh() }
        }
    }
}
