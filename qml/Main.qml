import QtQuick
import QtQuick.Window
Window{
    id:root
    visible:true
    flags:Qt.FramelessWindowHint|Qt.WindowStaysOnTopHint|Qt.Tool
    color:"transparent"
    property bool expanded:true
    readonly property int expandedWidth:0.25*Screen.width
    readonly property int collapsedWidth:4
    width:expanded?expandedWidth:collapsedWidth
    height:Screen.height
    x:Screen.width-width
    y:0
    Behavior on width{
        NumberAnimation{
            duration:250
            easing.type:Easing.OutCubic
        }
    }
    onWidthChanged:{x=Screen.width-width}
    Sidebar{
        anchors.fill:parent
        expanded:root.expanded
        onToggleRequested:{
            root.expanded=!root.expanded
        }
    }
}