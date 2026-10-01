import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: root
    property bool primary: false
    implicitWidth: Math.max(72,label.implicitWidth+24); implicitHeight: 36
    opacity: enabled ? 1 : 0.45
    contentItem: Text { id: label; text: root.text; color: root.primary ? "white" : themeManager.accent; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    background: Rectangle { radius: 10; color: root.primary ? themeManager.accent : root.hovered || root.down ? themeManager.accentSoft : themeManager.surfaceStrong; border.width: 1; border.color: root.activeFocus ? themeManager.accent : themeManager.border }
}
