import QtQuick

Item {
    id: root

    property string text: ""
    property color textColor: "white"
    property int pixelSize: 12
    property int fontWeight: Font.Normal
    property real scrollPixelsPerSecond: 34
    property int gap: 32
    readonly property bool overflow: firstText.implicitWidth > width
    readonly property real travelDistance: firstText.implicitWidth + gap

    function updateAnimation() {
        marqueeAnimation.stop()
        movingContent.x = 0
        if (root.visible && root.overflow)
            marqueeAnimation.start()
    }

    implicitHeight: firstText.implicitHeight
    clip: true

    Item {
        id: movingContent
        x: 0
        width: root.overflow ? root.travelDistance + secondText.implicitWidth
                             : firstText.implicitWidth
        height: root.height

        Text {
            id: firstText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.textColor
            font.pixelSize: root.pixelSize
            font.weight: root.fontWeight
            textFormat: Text.PlainText
        }

        Text {
            id: secondText
            anchors.left: firstText.right
            anchors.leftMargin: root.gap
            anchors.verticalCenter: parent.verticalCenter
            visible: root.overflow
            text: root.text
            color: root.textColor
            font.pixelSize: root.pixelSize
            font.weight: root.fontWeight
            textFormat: Text.PlainText
        }
    }

    SequentialAnimation {
        id: marqueeAnimation
        loops: Animation.Infinite

        PauseAnimation { duration: 900 }
        NumberAnimation {
            target: movingContent
            property: "x"
            from: 0
            to: -root.travelDistance
            duration: Math.max(2400, root.travelDistance / root.scrollPixelsPerSecond * 1000)
            easing.type: Easing.Linear
        }
        PauseAnimation { duration: 650 }
    }

    Component.onCompleted: updateAnimation()
    onTextChanged: updateAnimation()
    onWidthChanged: updateAnimation()
    onVisibleChanged: updateAnimation()
    onOverflowChanged: updateAnimation()
}
