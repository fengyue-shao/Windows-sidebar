import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "CardRegions.js" as Regions

Item {
    id: root
    required property var manager
    required property var componentProvider
    property string pageId: "monitor"
    property bool showAll: false
    property bool blocked: false
    property rect headerArea: Qt.rect(0,0,0,0)
    readonly property bool editing: manager.editing && manager.editingPage === pageId
    readonly property bool hasPinned: manager.hasPinnedCards
    property bool selectorOpen: false
    property string interactionHint: ""
    readonly property bool toolbarVisible: showAll || editing
    onEditingChanged: { if (!editing) selectorOpen=false; interactionHint="" }
    function configure() {
        manager.configureDesktop(width,height,[{x:headerArea.x,y:headerArea.y,width:headerArea.width,height:headerArea.height},
            {x:toolbar.x,y:toolbar.y,width:toolbar.width,height:toolbar.height}])
    }
    Component.onCompleted: configure()
    onWidthChanged: Qt.callLater(configure)
    onHeightChanged: Qt.callLater(configure)
    onHeaderAreaChanged: Qt.callLater(configure)
    function revealCard(id) { interactionHint=manager.desktopGeometry(id).visible ? "" : "该卡片已关闭或屏幕没有足够空间，可在编辑布局中重新开启或调整" }
    function inputRegions(target) {
        if(editing) {
            const p=root.mapToItem(target,0,0)
            return [{x:p.x,y:p.y,width:root.width,height:root.height}]
        }
        let items=[]
        for(let i=0;i<cards.count;++i) if(cards.itemAt(i)) items.push(cards.itemAt(i))
        if(toolbarVisible) items.push(toolbar)
        if(selector.visible) items.push(selector)
        if(message.visible) items.push(message)
        return Regions.collect(items,target,root)
    }
    function cardVisible(id) {
        const revision=manager.revision, g=manager.desktopGeometry(id)
        return !!g.visible && (showAll || editing || !!g.pinned)
    }
    Repeater {
        id: cards
        model: root.manager.catalog.filter(d=>d.page===root.pageId)
        delegate: Rectangle {
            id: frame
            required property var modelData
            objectName: "desktopCard-"+modelData.id
            readonly property var placement: { const revision=root.manager.revision; return root.manager.desktopGeometry(modelData.id) }
            property rect preview: Qt.rect(0,0,0,0)
            readonly property bool manipulating: drag.pressed || resize.pressed
            x: manipulating ? preview.x : (placement.x || 0)
            y: manipulating ? preview.y : (placement.y || 0)
            width: manipulating ? preview.width : (placement.width || 0)
            height: manipulating ? preview.height : (placement.height || 0)
            visible: !!placement.visible && (root.showAll || root.editing || !!placement.pinned)
            color: themeManager.cardSurface; radius: 16; clip: true
            border.width: root.editing ? 2 : 1
            border.color: root.editing ? themeManager.accent : themeManager.border
            function propose(rect) {
                if(root.manager.canPlaceDesktop(modelData.id,rect.x,rect.y,rect.width,rect.height)) {
                    preview=rect; root.interactionHint=""
                } else root.interactionHint="不能重叠、超出屏幕或遮挡顶部栏/编辑工具"
            }
            function finish() { root.manager.setDesktopGeometry(modelData.id,preview.x,preview.y,preview.width,preview.height) }
            Flickable {
                id: content
                anchors.fill: parent; anchors.topMargin: root.editing ? 34 : 0
                clip: true; boundsBehavior: Flickable.StopAtBounds; interactive: !root.editing
                contentWidth: width; contentHeight: Math.max(height,node.item ? node.item.implicitHeight : 0)
                Loader {
                    id: node
                    active: frame.visible
                    width: content.width; height: content.contentHeight
                    sourceComponent: root.componentProvider(frame.modelData.id)
                    enabled: !root.editing && !root.blocked
                }
            }
            MouseArea { anchors.fill: parent; enabled: root.editing || root.blocked }
            Rectangle {
                objectName: "desktopCardHeader-"+frame.modelData.id
                width: parent.width; height: 32; visible: root.editing
                radius: 12; color: themeManager.accentSoft
                Item {
                    anchors.left: parent.left; anchors.right: toggles.left; height: parent.height
                    Text { anchors.fill: parent; anchors.margins: 8; text: frame.modelData.title; color: themeManager.textPrimary; font.pixelSize: 11; elide: Text.ElideRight }
                    MouseArea {
                        id: drag
                        objectName: "desktopCardDrag-"+frame.modelData.id
                        anchors.fill: parent; cursorShape: Qt.SizeAllCursor; preventStealing: true
                        enabled: !root.blocked
                        property point origin
                        property rect initial
                        onPressed: mouse=> { origin=mapToItem(root,mouse.x,mouse.y); initial=Qt.rect(frame.x,frame.y,frame.width,frame.height); frame.preview=initial }
                        onPositionChanged: mouse=> {
                            if(!pressed) return
                            const p=mapToItem(root,mouse.x,mouse.y)
                            frame.propose(Qt.rect(Math.max(0,Math.min(root.width-initial.width,initial.x+p.x-origin.x)),Math.max(0,Math.min(root.height-initial.height,initial.y+p.y-origin.y)),initial.width,initial.height))
                        }
                        onReleased: frame.finish()
                        onCanceled: root.interactionHint=""
                    }
                }
                Row {
                    id: toggles
                    anchors.right: parent.right; anchors.rightMargin: 4; height: parent.height; spacing: 2
                    StateToggle { objectName: "desktopCardEnabled-"+frame.modelData.id; text: "开启"; checked: !!frame.placement.enabled; enabled: !root.blocked; onClicked: root.manager.setEnabled(frame.modelData.id,!frame.placement.enabled) }
                    StateToggle { objectName: "desktopCardPinned-"+frame.modelData.id; text: "固定"; checked: !!frame.placement.pinned; enabled: !root.blocked; onClicked: root.manager.setPinned(frame.modelData.id,!frame.placement.pinned) }
                }
            }
            Rectangle {
                anchors.right: parent.right; anchors.bottom: parent.bottom; width: 24; height: 24; radius: 8
                visible: root.editing; color: themeManager.accent
                Text { anchors.centerIn: parent; text: "+"; color: "#10151D" }
                MouseArea {
                    id: resize
                    objectName: "desktopCardResize-"+frame.modelData.id
                    anchors.fill: parent; cursorShape: Qt.SizeFDiagCursor; preventStealing: true
                    enabled: !root.blocked
                    property point origin
                    property rect initial
                    onPressed: mouse=> { origin=mapToItem(root,mouse.x,mouse.y); initial=Qt.rect(frame.x,frame.y,frame.width,frame.height); frame.preview=initial }
                    onPositionChanged: mouse=> {
                        if(!pressed) return
                        const p=mapToItem(root,mouse.x,mouse.y)
                        frame.propose(Qt.rect(initial.x,initial.y,Math.max(Math.min(frame.modelData.minimumWidth,root.width),Math.min(root.width-initial.x,initial.width+p.x-origin.x)),Math.max(Math.min(frame.modelData.minimumHeight,root.height),Math.min(frame.modelData.maximumHeight,root.height-initial.y,initial.height+p.y-origin.y))))
                    }
                    onReleased: frame.finish()
                    onCanceled: root.interactionHint=""
                }
            }
        }
    }
    Rectangle {
        id: toolbar
        enabled: !root.blocked
        objectName: "desktopCardToolbar"
        z: 100
        width: Math.min(620,root.width-16); height: 50
        x: (root.width-width)/2; y: root.height-height-8
        visible: root.toolbarVisible
        radius: 14; color: themeManager.cardSurface; border.color: themeManager.border
        RowLayout {
            anchors.fill: parent; anchors.margins: 8; spacing: 4
            Text { Layout.fillWidth: true; text: root.editing ? "编辑画布接管输入 · 拖动/拉伸 · 禁止重叠" : "屏幕自由布局"; color: themeManager.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
            ActionButton { objectName: "desktopLayoutEdit"; text: "编辑布局"; visible: !root.editing; onClicked: root.manager.beginDesktopEdit(root.pageId) }
            ActionButton { objectName: "desktopLayoutCards"; text: "卡片"; visible: root.editing; onClicked: root.selectorOpen=!root.selectorOpen }
            ActionButton { objectName: "desktopLayoutReset"; text: "默认"; visible: root.editing; onClicked: root.manager.resetDraft() }
            ActionButton { objectName: "desktopLayoutCancel"; text: "取消"; visible: root.editing; onClicked: root.manager.cancelEdit() }
            ActionButton { objectName: "desktopLayoutSave"; text: "保存"; visible: root.editing; onClicked: root.manager.saveEdit() }
        }
    }
    Rectangle {
        id: selector
        enabled: !root.blocked
        z: 101
        width: Math.min(280,root.width-16); height: Math.min(root.height-80,choices.implicitHeight+16)
        x: toolbar.x+toolbar.width-width; y: toolbar.y-height-8
        visible: root.editing && root.selectorOpen
        radius: 14; color: themeManager.cardSurface; border.color: themeManager.border
        Flickable {
            anchors.fill: parent; anchors.margins: 8; clip: true; contentHeight: choices.implicitHeight
            Column {
                id: choices; width: parent.width
                Repeater {
                    model: root.manager.catalog.filter(d=>d.page===root.pageId)
                    delegate: StateToggle {
                        required property var modelData
                        objectName: "desktopRestore-"+modelData.id
                        readonly property var placement: { const revision=root.manager.revision; return root.manager.desktopGeometry(modelData.id) }
                        width: choices.width
                        checked: !!placement.enabled
                        text: modelData.title + (!placement.available ? " · 模块关闭" : !placement.placed && placement.enabled ? " · 空间不足" : "")
                        onClicked: root.manager.setEnabled(modelData.id,!placement.enabled)
                    }
                }
            }
        }
    }
    Rectangle {
        id: message
        z: 102
        width: toolbar.width; height: hint.implicitHeight+16
        x: toolbar.x; y: toolbar.y-height-8
        visible: root.toolbarVisible && !selector.visible && hint.text.length>0
        color: themeManager.cardSurface; radius: 10; border.color: themeManager.border
        Text { id: hint; x: 8; y: 8; width: parent.width-16; text: root.manager.lastError || root.interactionHint; color: themeManager.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; textFormat: Text.PlainText }
    }
    component ActionButton: Basic.Button {
        implicitWidth: 64; implicitHeight: 34
        contentItem: Text { text: parent.text; color: themeManager.textPrimary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: parent.hovered || parent.down ? themeManager.accentSoft : themeManager.cardSurface; border.color: themeManager.border }
    }
    component StateToggle: ActionButton {
        implicitWidth: 58; implicitHeight: 30
        contentItem: Text { text: (parent.checked ? "✓ " : "○ ")+parent.text; color: parent.checked ? themeManager.accent : themeManager.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    }
}
