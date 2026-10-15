import QtQuick
import QtQuick.Controls.Basic as Basic
import "../qml"

Item {
    id: fixture
    required property var manager
    property int actions: 0
    property bool expanded: true
    property alias canvas: canvas
    Component.onCompleted: {
        for(const id of ["first","second"])
            manager.registerCard({id:id,page:"monitor",title:id,minimumWidth:180,minimumHeight:100,defaultHeight:180,maximumHeight:2048})
    }
    DesktopCardCanvas {
        id: canvas
        anchors.fill: parent
        manager: fixture.manager
        showAll: fixture.expanded
        headerArea: Qt.rect((width-300)/2,0,300,100)
        componentProvider: function(id) { return content }
    }
    Component {
        id: content
        Rectangle {
            implicitHeight: 240
            color: "#202A38"
            Basic.Button { objectName: "desktopBusiness"; x: 12; y: 20; text: "业务按钮"; onClicked: fixture.actions++ }
            Text { x:12; y:100; text: "可自由拉伸 · 小尺寸内部滚动"; color: "white" }
        }
    }
}
