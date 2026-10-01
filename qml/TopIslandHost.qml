import QtQuick

Item {
    id: root
    property Component fallbackContent
    property Component islandContent: null
    readonly property var activeState: islandBridge.activeState
    signal cardsToggleRequested()
    // Keep the existing status layout until an island renderer is designed.
    Loader { anchors.fill: parent; sourceComponent: root.islandContent || root.fallbackContent }
    TapHandler {
        onDoubleTapped: root.cardsToggleRequested()
        onSingleTapped: islandBridge.requestDetails()
    }
}
