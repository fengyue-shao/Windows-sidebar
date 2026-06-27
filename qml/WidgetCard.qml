import QtQuick

Rectangle {
    id: root

    property string title: ""
    property int cardHeight: 80

    width: parent.width
    height: cardHeight

    radius: 12

    color: "#303030"

    Text {
        anchors.centerIn: parent

        text: root.title

        color: "white"

        font.pixelSize: 18
    }
}