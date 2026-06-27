import QtQuick
import QtQuick.Controls

Item {
    id:root
    property bool expanded:false
    signal toggleRequested()
    Rectangle{
        id:sidebar
        anchors.fill:parent
        radius:0
        color:"#DD202020"
        border.color:"#404040"
    }
    MouseArea{
        anchors.fill:parent
        onClicked:{
            if(!expanded)
                root.toggleRequested()
        }
    }
    Rectangle{
        id:statusButton
        width:24
        height:80
        radius:12
        color:"#505050"
        anchors.left:parent.left
        anchors.verticalCenter:parent.verticalCenter
        Text{
            anchors.centerIn: parent
            text:expanded?">":"<"
            color:"white"
            font.pixelSize:18
        }
        MouseArea{
            anchors.fill:parent
            onClicked:{
                root.toggleRequested()
            }
        }
    }
    Timebar{
        id:timebar
        visible:expanded
        anchors.top: parent.top
        color:sidebar.color
    }

    Column{
        visible: expanded

        anchors {
            top: timebar.bottom
            left: parent.left
            right: parent.right

            margins: 20
            leftMargin: 40
        }

        spacing: 12


        Column{
            spacing:6
            Row{
                spacing:12
                Image{
                    width:80
                    height:80
                    source:"image://cover/current?"+mediaMonitor.coverVersion
                    fillMode:Image.PreserveAspectCrop
                }
                Column{
                    spacing:6
                    Text{
                       id:mediatitle
                       text:mediaMonitor.title
                       color:"white"
                    }
                    Text{
                        text:mediaMonitor.artist
                        color:"#AAAAAA"
                    }
                    Text{
                        text:mediaMonitor.album
                        color:"#AAAAAA"
                    }
                }
            }

            Text{
                text:mediaMonitor.playing?"⏸":"▶"
                color:"lightgreen"
            }
        }
        Row{
            spacing:10
            WidgetCard {
                id:cpumonitor
                width:100
                title: "CPU: "+cpuMonitor.usage.toFixed(0)+"%"
            }
            WidgetCard{
                id:memorymonitor
                width:200
                title:"Memory: "+memoryMonitor.usage.toFixed(0)+"%"
            }
        }
        WidgetCard{
            id:hnetmonitor
            width:200
            title:"↑: "+networkMonitor.uploadSpeedText
        }
        WidgetCard{
            id:lnetmonitor
            width:200
            title:"↓: "+networkMonitor.downloadSpeedText
        }

        WidgetCard {
            id:messagebar
           title: "Custom"
          cardHeight: 200
        }
    }
}
