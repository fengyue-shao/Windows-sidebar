import QtQuick

Item {
    id: root
    property var bands: []
    property bool active: false
    property color barColor: themeManager.textPrimary
    readonly property alias barItems: bars
    Repeater {
        id: bars
        model: 32
        Rectangle {
            required property int index
            readonly property real value: root.active && index < root.bands.length && isFinite(root.bands[index]) ? Math.max(0, Math.min(1, root.bands[index])) : 0
            x: (index+0.5)*root.width/32-width/2
            y: (root.height-height)/2
            width: Math.max(2, Math.min(6, root.width/32*0.5))
            height: 2+value*(root.height-2)
            radius: 1
            color: root.barColor
            Behavior on height { NumberAnimation { duration: 55 } }
            Behavior on y { NumberAnimation { duration: 55 } }
        }
    }
}
