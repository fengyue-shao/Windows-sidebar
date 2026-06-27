import QtQuick

Rectangle {
    id: root
    property string title: "timebar"
    height: 80
    width: parent.width
    property string currentTime
    property string currentDateandWeek
    Timer {
        interval: 1000
        running: true
        repeat: true

        onTriggered: {
            currentTime = Qt.formatTime(new Date(), "hh:mm:ss");
            currentDateandWeek=new Date().toLocaleDateString(Qt.locale(),"dddd,MMMM d,yyyy")
        }
    }
    Text {
        anchors{
            left:parent.left
            top:parent.top
            topMargin: 10.0
            leftMargin: 20.0
        }
        id:currenttime
        font.pixelSize: 30
        text:currentTime
        color:"white"
    }
    Text{
        anchors{
            left:parent.left
            leftMargin:20.0
            top:currenttime.bottom
        }
        id:dateandweek
        font.pixelSize:15
        color:"white"
        text: currentDateandWeek
    }
}
