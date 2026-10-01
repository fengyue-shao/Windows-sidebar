import QtQuick
import QtQuick.Controls

CardSurface {
    id: root
    property string label
    property string description
    property int minutes
    property bool selected: false
    signal chosen()
    implicitHeight: 96
    border.color: selected ? themeManager.accent : themeManager.border
    Rectangle { anchors.fill: parent; radius: parent.radius; color: themeManager.accentSoft; opacity: root.selected ? 0.6 : hover.hovered ? 0.3 : 0 }
    Text { x: 16; y: 14; text: root.label; color: root.selected ? themeManager.accent : themeManager.textPrimary; font.pixelSize: 12; font.weight: Font.DemiBold }
    Text { x: 16; y: 35; text: root.description; color: themeManager.textMuted; font.pixelSize: 10 }
    Text { anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; text: root.minutes + " 分钟"; color: themeManager.accent; font.pixelSize: 19 }
    Text { x: 16; y: 70; text: root.selected ? "● 当前节奏" : "点击切换节奏"; color: themeManager.textFaint; font.pixelSize: 9 }
    HoverHandler { id: hover }
    TapHandler { onTapped: root.chosen() }
    ToolTip.visible: hover.hovered
    ToolTip.text: "切换节奏会重置当前计时"
}
