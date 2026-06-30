import QtQuick

Rectangle{
    id:root
    property int cardHeight: 80

    width: parent.width
    height: cardHeight

    radius: 12

    color: "#303030"
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
            Row{
                spacing:10
                Text{
                    text:mediaMonitor.playing?"⏸":"▶"
                    color:"lightgreen"
                    MouseArea{
                        anchors.fill:parent
                        onClicked:mediaMonitor.togglePlayPause()
                    }
                }
                Text{
                    text: ">|"
                    color:"white"
                    MouseArea{
                        anchors.fill:parent
                        onClicked:mediaMonitor.next()
                    }
                }
                Text{
                    text:"|<"
                    color:"white"
                    MouseArea{
                        anchors.fill:parent
                        onClicked:mediaMonitor.previous()
                    }
                }
            }
        }
    }
}
