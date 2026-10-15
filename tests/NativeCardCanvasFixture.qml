import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "../qml"

Item {
    id: fixture
    required property var manager
    property string metricValue: "35%"
    property int actions: 0
    Component.onCompleted: {
        for (const id of ["first", "second"])
            manager.registerCard({id: id, page: "monitor", title: id === "first" ? "系统状态" : "任务进度",
                minimumWidth: 200, minimumHeight: 180, defaultHeight: 200, maximumHeight: 700})
    }
    NativeCardCanvas {
        anchors.fill: parent
        manager: fixture.manager
        componentProvider: function(id) { return id === "first" ? first : second }
    }
    component FixtureCard: Rectangle {
        required property string cardId
        implicitHeight: 180
        color: "#202A38"
        radius: 16
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 12
            Text { text: "QML 原生卡片"; color: "#EDF2FA"; font.pixelSize: 18 }
            Text { objectName: cardId === "first" ? "fixtureMetric" : ""; text: fixture.metricValue; color: "#72A7FF"; font.pixelSize: 24 }
            Basic.Button {
                objectName: "managedAction-"+cardId+"-refresh"
                text: "刷新"
                onClicked: fixture.actions++
            }
            Item { Layout.fillHeight: true }
        }
    }
    Component { id: first; FixtureCard { cardId: "first" } }
    Component { id: second; FixtureCard { cardId: "second" } }
}
