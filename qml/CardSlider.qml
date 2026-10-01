import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Slider {
    id: root
    implicitHeight: 28; implicitWidth: 160
    background: Rectangle {
        x: root.leftPadding; y: root.topPadding+root.availableHeight/2-height/2
        width: root.availableWidth; height: 4; radius: 2; color: themeManager.track
        Rectangle { width: parent.width*root.visualPosition; height: parent.height; radius: 2; color: root.enabled ? themeManager.accent : themeManager.textFaint }
    }
    handle: Rectangle {
        x: root.leftPadding+root.visualPosition*(root.availableWidth-width)
        y: root.topPadding+root.availableHeight/2-height/2
        width: 14; height: 14; radius: 7
        color: root.enabled ? themeManager.accent : themeManager.textFaint
        border.width: root.activeFocus ? 2 : 0; border.color: themeManager.textPrimary
    }
}
