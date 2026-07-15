import QtQuick

Rectangle{
    id:root
    property int cardHeight: 100
    visible:mediatitle.text===""?false:true
    width: 0.9*parent.width
    height: cardHeight

    radius: 12
    HoverHandler{
        id:mediahoverhandler
    }

    color: mediahoverhandler.hovered?"#28FFFFFF":"#20FFFFFF"
    border.width:1
    border.color: "#35FFFFFF"
    Row{
        id:body
        anchors{
            left:parent.left
            leftMargin: 10
            top:parent.top
            topMargin: 10
            bottom:parent.bottom
            bottomMargin: 10
        }

        spacing:12
        Image{
            id:mediaimg
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
               font.pixelSize: 14
               font.bold: true
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
    Row{
        anchors{
            right:root.right
            rightMargin: 20
            bottom:body.bottom
        }

        spacing:(root.width-100)/3
        Image{
            width:20
            height:20
            source: Qt.resolvedUrl("assets/icons/ic_fluent_previous_48_regular.svg")
            MouseArea{
                anchors.fill:parent
                onClicked:mediaMonitor.previous()
            }
        }
        Image{
            width:20
            height:20
            source: mediaMonitor.playing?Qt.resolvedUrl("assets/icons/ic_fluent_pause_48_regular.svg"):Qt.resolvedUrl("assets/icons/ic_fluent_play_48_regular.svg")
            MouseArea{
                anchors.fill:parent
                onClicked:mediaMonitor.togglePlayPause()
            }
        }
        Image {
            width:20
            height:20
            source:Qt.resolvedUrl("assets/icons/ic_fluent_next_48_regular.svg")
            MouseArea{
                anchors.fill:parent
                onClicked:mediaMonitor.next()
            }
        }
    }
}
