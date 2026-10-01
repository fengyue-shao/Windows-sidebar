import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "CardRegions.js" as Regions

Item {
    id: root
    function inputRegions(target) { return Regions.collect([toolbar, empty], target, root).concat(Regions.collect(Regions.delegates(history), target, history)) }

    onVisibleChanged: if (visible) clipboardManager.refresh()
    Component.onCompleted: if (visible) clipboardManager.refresh()

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        CardSurface {
            id: toolbar
            Layout.fillWidth: true
            Layout.preferredHeight: header.implicitHeight + 32
            ColumnLayout {
                id: header
                anchors.fill: parent; anchors.margins: 16; spacing: 6
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "剪贴板"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
            SmallButton { symbol: clipboardManager.loading ? "…" : "↻"; tip: "刷新"; enabled: !clipboardManager.loading; onClicked: clipboardManager.refresh() }
            SmallButton { symbol: "×"; tip: "清空非固定历史"; danger: true; onClicked: clipboardManager.clearHistory() }
        }

        Text {
            Layout.fillWidth: true
            text: clipboardManager.statusText
            color: themeManager.textMuted
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
            }
        }

        CardSurface {
            id: empty
            Layout.fillWidth: true; Layout.preferredHeight: 100
            visible: history.count === 0
            Text { anchors.centerIn: parent; text: clipboardManager.loading ? "正在读取剪贴板…" : "暂无可预览的历史"; color: themeManager.textMuted; font.pixelSize: 12 }
        }

        ListView {
            id: history
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            model: clipboardManager.items
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                readonly property bool cardDelegate: true
                required property var modelData
                width: ListView.view.width
                height: modelData.type === "image" ? 174 : Math.max(76, previewText.implicitHeight + 42)
                radius: 16
                color: themeManager.cardSurface
                border.width: 1
                border.color: itemHover.hovered ? themeManager.accent : themeManager.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 7
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle {
                            Layout.preferredWidth: 26; Layout.preferredHeight: 26; radius: 8
                            color: themeManager.accentSoft
                            Text { anchors.centerIn: parent; text: modelData.type === "image" ? "▧" : modelData.type === "files" ? "▤" : "T"; color: themeManager.accent; font.pixelSize: 10; font.weight: Font.Bold }
                        }
                        Text { Layout.fillWidth: true; text: modelData.detail; color: themeManager.textMuted; font.pixelSize: 9 }
                        Rectangle {
                            Layout.preferredWidth: 28; Layout.preferredHeight: 26; radius: 8
                            color: modelData.pinned ? themeManager.accentSoft : pinHover.hovered ? themeManager.surfaceStrong : themeManager.overlayStart
                            Text { anchors.centerIn: parent; text: modelData.pinned ? "◆" : "◇"; color: modelData.pinned ? themeManager.accent : themeManager.textMuted; font.pixelSize: 11 }
                            HoverHandler { id: pinHover }
                            TapHandler { onTapped: clipboardManager.togglePinned(modelData.id) }
                            ToolTip.visible: pinHover.hovered
                            ToolTip.text: modelData.pinned ? "取消应用内置顶" : "应用内置顶"
                        }
                        Rectangle {
                            Layout.preferredWidth: 28; Layout.preferredHeight: 26; radius: 8
                            color: deleteHover.hovered ? themeManager.dangerSoft : themeManager.overlayStart
                            Text { anchors.centerIn: parent; text: "×"; color: themeManager.danger; font.pixelSize: 14 }
                            HoverHandler { id: deleteHover }
                            TapHandler {
                                grabPermissions: PointerHandler.CanTakeOverFromAnything
                                onTapped: clipboardManager.deleteItem(modelData.id)
                            }
                            ToolTip.visible: deleteHover.hovered
                            ToolTip.text: "删除这一项"
                        }
                    }
                    Image {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: modelData.type === "image"
                        source: modelData.imageSource || ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Text {
                        id: previewText
                        Layout.fillWidth: true
                        visible: modelData.type !== "image"
                        text: modelData.preview
                        color: themeManager.textPrimary
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }
                }
                HoverHandler { id: itemHover }
                TapHandler { onTapped: clipboardManager.activateItem(modelData.id) }
            }

        }
    }

    component SmallButton: Rectangle {
        id: button
        property string symbol
        property string tip
        property bool danger: false
        signal clicked()
        implicitWidth: 32; implicitHeight: 30; radius: 10
        opacity: enabled ? 1 : 0.45
        color: buttonTap.pressed ? themeManager.overlayStart : buttonHover.hovered ? (danger ? themeManager.dangerSoft : themeManager.accentSoft) : themeManager.surface
        border.width: 1; border.color: danger ? themeManager.dangerSoft : themeManager.border
        Text { anchors.centerIn: parent; text: button.symbol; color: button.danger ? themeManager.danger : themeManager.accent; font.pixelSize: 12 }
        HoverHandler { id: buttonHover }
        TapHandler { id: buttonTap; enabled: button.enabled; onTapped: button.clicked() }
        ToolTip.visible: buttonHover.hovered
        ToolTip.text: button.tip
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
