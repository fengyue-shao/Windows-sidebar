import QtQuick

Item {
    id: root
    property string name
    property color tint: themeManager.textSecondary
    implicitWidth: 20; implicitHeight: 20
    Image {
        anchors.fill: parent
        sourceSize.width: 40; sourceSize.height: 40
        fillMode: Image.PreserveAspectFit
        source: root.name ? "image://fluent/" + root.name + "/" + root.tint.toString().substring(1) : ""
    }
}
