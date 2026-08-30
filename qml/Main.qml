import QtQuick
import QtQuick.Window

Window {
    id: root

    visible: true
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"

    property bool expanded: true
    readonly property int expandedWidth: Math.min(440, Math.max(410, displayManager.selectedWidth * 0.225))
    readonly property int collapsedWidth: 6

    width: expanded ? expandedWidth : collapsedWidth
    height: displayManager.selectedHeight
    x: displayManager.dockSide === 0
       ? displayManager.selectedX
       : displayManager.selectedX + displayManager.selectedWidth - width
    y: displayManager.selectedY

    Behavior on width {
        NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
    }

    Sidebar {
        anchors.fill: parent
        expanded: root.expanded
        onToggleRequested: root.expanded = !root.expanded
    }
}
