import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "CardRegions.js" as Regions

Item {
    id: root
    required property var manager
    required property var componentProvider
    property string pageId: "monitor"
    readonly property bool editing: manager.editing && manager.editingPage === pageId
    readonly property bool interacting: editing
    property bool selectorOpen: false
    onEditingChanged: if (!editing) selectorOpen = false
    function inputRegions(target) {
        if (editing) { const p=root.mapToItem(target,0,0); return [{x:p.x,y:p.y,width:width,height:height}] }
        let items=[]
        for(let i=0;i<cards.count;++i) if(cards.itemAt(i)) items.push(cards.itemAt(i))
        return Regions.collect([toolbar],target,root).concat(Regions.collect(items,target,viewport))
    }
    function revealCard(id) {
        const g=manager.geometry(id,viewport.width)
        if(g.visible) viewport.contentY=Math.max(0,Math.min(g.y,Math.max(0,viewport.contentHeight-viewport.height)))
    }
    Rectangle {
        id: toolbar
        width: parent.width; height: 50; radius: 14; color: themeManager.cardSurface; border.color: themeManager.border
        RowLayout {
            anchors.fill: parent; anchors.margins: 8; spacing: 6
            Text { Layout.fillWidth: true; text: root.editing ? "拖动标题 · 右下角拉伸" : "自定义卡片布局 · 接口预览"; color: themeManager.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
            EditButton { objectName: "cardLayoutEdit"; visible: !root.editing; text: "编辑布局"; onClicked: root.manager.beginEdit(root.pageId,viewport.width) }
            EditButton { objectName: "cardLayoutVisibility"; visible: root.editing; text: "卡片"; onClicked: root.selectorOpen = !root.selectorOpen }
            EditButton { objectName: "cardLayoutReset"; visible: root.editing; text: "默认"; onClicked: root.manager.resetDraft() }
            EditButton { objectName: "cardLayoutCancel"; visible: root.editing; text: "取消"; onClicked: root.manager.cancelEdit() }
            EditButton { objectName: "cardLayoutSave"; visible: root.editing; text: "保存"; onClicked: root.manager.saveEdit() }
        }
    }
    Rectangle {
        id: selector
        objectName: "cardVisibilityPanel"
        y: toolbar.y + toolbar.height + 8; width: parent.width
        height: visible ? choices.implicitHeight + 16 : 0
        visible: root.editing && root.selectorOpen
        radius: 14; color: themeManager.cardSurface; border.color: themeManager.border
        Flow {
            id: choices
            x: 8; y: 8; width: parent.width - 16; spacing: 4
            Repeater {
                model: root.manager.catalog.filter(d=>d.page===root.pageId)
                delegate: Basic.CheckBox {
                    id: visibilityChoice
                    required property var modelData
                    objectName: "cardVisibility-" + modelData.id
                    readonly property var placement: { const revision=root.manager.revision; return root.manager.geometry(modelData.id,viewport.width) }
                    width: Math.min(180, choices.width)
                    text: modelData.title + (placement.available ? "" : "（模块已关闭）")
                    checked: !!placement.userVisible
                    // Visibility is a layout preference, not a module/service switch.
                    onClicked: root.manager.setVisible(modelData.id,checked)
                    indicator: Rectangle {
                        x: 4; y: (visibilityChoice.height-height)/2
                        width: 20; height: 20; radius: 6
                        color: visibilityChoice.checked ? themeManager.accent : themeManager.cardSurface
                        border.color: visibilityChoice.checked ? themeManager.accent : themeManager.border
                        Text { anchors.centerIn: parent; text: "✓"; visible: visibilityChoice.checked; color: "#10151D"; font.pixelSize: 14; font.bold: true }
                    }
                    contentItem: Text { leftPadding: 28; text: parent.text; color: themeManager.textPrimary; font.pixelSize: 11; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
    }
    Text { id: error; y: (selector.visible ? selector.y+selector.height : toolbar.y+toolbar.height)+4; width: parent.width; text: root.manager.lastError; visible: text.length>0; color: themeManager.danger; font.pixelSize: 10; wrapMode: Text.Wrap; textFormat: Text.PlainText }
    Flickable {
        id: viewport
        anchors.top: error.visible ? error.bottom : (selector.visible ? selector.bottom : toolbar.bottom); anchors.topMargin: 12
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        contentWidth: width
        contentHeight: { const revision=root.manager.revision; return root.manager.contentHeight(root.pageId,width)+16 }
        clip: true; boundsBehavior: Flickable.StopAtBounds
        interactive: !root.editing
        Repeater {
            id: cards
            model: root.manager.catalog.filter(d=>d.page===root.pageId)
            delegate: Rectangle {
                id: frame
                required property var modelData
                objectName: "managedCard-"+modelData.id
                readonly property var placement: { const revision=root.manager.revision; return root.manager.geometry(modelData.id,viewport.width) }
                property real previewX: 0
                property real previewY: 0
                property real previewWidth: 0
                property real previewHeight: 0
                x: drag.pressed ? previewX : (placement.x || 0)
                y: drag.pressed ? previewY : (placement.y || 0)
                width: resize.pressed ? previewWidth : (placement.width || 0)
                height: resize.pressed ? previewHeight : (placement.height || 0)
                visible: !!placement.visible
                radius: 16; color: themeManager.cardSurface
                clip: true
                border.width: root.editing ? 2 : 0; border.color: themeManager.accent
                z: drag.pressed || resize.pressed ? 10 : 0
                Loader {
                    id: node
                    active: frame.visible
                    anchors.fill: parent
                    anchors.topMargin: root.editing ? 32 : 0
                    sourceComponent: root.componentProvider(frame.modelData.id)
                    enabled: !root.editing
                    onLoaded: sizeReport.restart()
                    function reportSize() {
                        if(item) root.manager.reportMinimumHeight(frame.modelData.id, item.implicitHeight+(root.editing ? 32 : 0))
                    }
                    // Owned timer prevents callbacks surviving a replaced delegate during registration.
                    Timer { id: sizeReport; interval: 0; onTriggered: node.reportSize() }
                    Connections {
                        target: node.item
                        function onImplicitHeightChanged() { sizeReport.restart() }
                    }
                    Connections {
                        target: root
                        function onEditingChanged() { sizeReport.restart() }
                    }
                }
                // Editing owns the entire surface: no underlying business action is triggered.
                MouseArea { anchors.fill: parent; enabled: root.editing; onWheel: wheel=> { viewport.contentY=Math.max(0,Math.min(viewport.contentY-wheel.angleDelta.y,Math.max(0,viewport.contentHeight-viewport.height))) } }
                Rectangle {
                    width: parent.width; height: 28; radius: 12; visible: root.editing; color: themeManager.accentSoft
                    Text { anchors.centerIn: parent; text: frame.modelData.title; color: themeManager.textPrimary; font.pixelSize: 10 }
                    MouseArea {
                        id: drag
                        objectName: "cardDrag-"+frame.modelData.id
                        anchors.fill: parent; cursorShape: Qt.SizeAllCursor; preventStealing: true
                        property point origin
                        property point initial
                        onPressed: mouse=> { origin=mapToItem(viewport.contentItem,mouse.x,mouse.y); initial=Qt.point(frame.x,frame.y); frame.previewX=frame.x; frame.previewY=frame.y }
                        onPositionChanged: mouse=> { if(!pressed) return; const p=mapToItem(viewport.contentItem,mouse.x,mouse.y); frame.previewX=Math.max(0,Math.min(initial.x+p.x-origin.x,viewport.width-frame.width)); frame.previewY=Math.max(0,initial.y+p.y-origin.y) }
                        onReleased: root.manager.setGeometry(frame.modelData.id,frame.previewX,frame.previewY,frame.width,frame.height,viewport.width)
                    }
                }
                Rectangle {
                    anchors.right: parent.right; anchors.bottom: parent.bottom; width: 24; height: 24; radius: 8
                    visible: root.editing; color: themeManager.accent
                    Text { anchors.centerIn: parent; text: "+"; color: "white" }
                    MouseArea {
                        id: resize
                        objectName: "cardResize-"+frame.modelData.id
                        anchors.fill: parent; cursorShape: Qt.SizeFDiagCursor; preventStealing: true
                        property point origin
                        property point initial
                        onPressed: mouse=> { origin=mapToItem(viewport.contentItem,mouse.x,mouse.y); initial=Qt.point(frame.width,frame.height); frame.previewWidth=frame.width; frame.previewHeight=frame.height }
                        onPositionChanged: mouse=> { if(!pressed) return; const p=mapToItem(viewport.contentItem,mouse.x,mouse.y); frame.previewWidth=Math.max(Math.min(frame.modelData.minimumWidth,viewport.width),Math.min(initial.x+p.x-origin.x,viewport.width-frame.x)); frame.previewHeight=Math.max(frame.modelData.minimumHeight,Math.min(frame.modelData.maximumHeight,initial.y+p.y-origin.y)) }
                        onReleased: root.manager.setGeometry(frame.modelData.id,frame.x,frame.y,frame.previewWidth,frame.previewHeight,viewport.width)
                    }
                }
            }
        }
    }
    component EditButton: Basic.Button {
        implicitWidth: 64; implicitHeight: 34
        contentItem: Text { text: parent.text; color: themeManager.textPrimary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: parent.down || parent.hovered ? themeManager.accentSoft : themeManager.cardSurface; border.width: 1; border.color: themeManager.border }
    }
}
