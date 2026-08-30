import QtQuick
import QtQuick.Window

Window {
    id: root

    visible: true
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"

    property bool expanded: uiSettings.expanded
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
        onToggleRequested: uiSettings.expanded = !uiSettings.expanded
    }

    Shortcut { sequence: "Ctrl+Space"; onActivated: uiSettings.expanded = !uiSettings.expanded }
    Shortcut { sequence: "Ctrl+1"; onActivated: { uiSettings.currentSection = 0; uiSettings.expanded = true } }
    Shortcut { sequence: "Ctrl+2"; onActivated: { uiSettings.currentSection = 1; uiSettings.expanded = true } }
    Shortcut { sequence: "Ctrl+3"; onActivated: { uiSettings.currentSection = 2; uiSettings.expanded = true } }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
}
