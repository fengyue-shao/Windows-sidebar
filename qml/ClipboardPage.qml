import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    onVisibleChanged: if (visible) clipboardManager.refresh()
    Component.onCompleted: if (visible) clipboardManager.refresh()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

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

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: clipboardManager.items
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: Math.max(66, previewText.implicitHeight + 30)
                radius: 15
                color: itemHover.hovered ? themeManager.surfaceHover : themeManager.surface
                border.width: 1
                border.color: itemHover.hovered ? themeManager.accent : themeManager.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    Rectangle {
                        Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 9
                        color: themeManager.accentSoft
                        Text { anchors.centerIn: parent; text: "T"; color: themeManager.accent; font.pixelSize: 10; font.weight: Font.Bold }
                    }
                    Text {
                        id: previewText
                        Layout.fillWidth: true
                        text: modelData.preview
                        color: themeManager.textPrimary
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }
                    Text { text: "复制"; color: themeManager.accent; font.pixelSize: 9; font.weight: Font.DemiBold }
                }
                HoverHandler { id: itemHover }
                TapHandler { onTapped: clipboardManager.copyText(modelData.text) }
            }

            Text {
                anchors.centerIn: parent
                visible: parent.count === 0 && !clipboardManager.loading
                text: "暂无文本历史"
                color: themeManager.textFaint
                font.pixelSize: 11
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
        color: buttonHover.hovered ? (danger ? themeManager.dangerSoft : themeManager.accentSoft) : themeManager.surface
        border.width: 1; border.color: danger ? themeManager.dangerSoft : themeManager.border
        Text { anchors.centerIn: parent; text: button.symbol; color: button.danger ? themeManager.danger : themeManager.accent; font.pixelSize: 12 }
        HoverHandler { id: buttonHover }
        TapHandler { onTapped: button.clicked() }
        ToolTip.visible: buttonHover.hovered
        ToolTip.text: button.tip
    }
}
