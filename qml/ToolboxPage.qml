import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import "CardRegions.js" as Regions

Item {
    id: root
    property bool versionPopupOpen: false
    readonly property bool popupOpen: trustDialog.visible || folderDialog.visible || versionDialog.visible || versionPopupOpen
    function closePopups() {
        trustDialog.close(); folderDialog.close(); versionDialog.close()
        for (let i=0; i<plugins.count; ++i) if (plugins.itemAt(i)) plugins.itemAt(i).versionPopup.close()
    }
    function scheduleCapture(path) { if (path) Qt.callLater(() => root.grabToImage(result => result.saveToFile(path + ".toolbox.png"))) }
    function statusLabel(value) {
        return ({stopped:"未运行",starting:"启动中",running:"运行中",stopping:"停止中",crashed:"异常退出",disconnected:"连接中断",
            "protocol-error":"接口校验失败","start-failed":"启动失败","listen-failed":"通信建立失败","handshake-timeout":"连接超时"})[value] || value
    }
    FolderDialog {
        id: folderDialog
        title: "选择可信的独立插件包文件夹"
        onAccepted: { trustDialog.pluginId = ""; trustDialog.packageFolder = selectedFolder.toString(); trustDialog.open() }
    }
    Basic.Dialog {
        id: versionDialog
        objectName: "pluginVersionConfirmation"
        property string pluginId: ""
        property string version: ""
        title: "确认切换插件版本"
        modal: true
        width: Math.min(440, root.width - 24)
        x: (root.width-width)/2; y: Math.max(12,(root.height-height)/2)
        background: Rectangle { radius: 16; color: themeManager.surface; border.color: themeManager.border; border.width: 1 }
        header: Text { text: versionDialog.title; color: themeManager.textPrimary; font.pixelSize: 15; padding: 16 }
        contentItem: Text {
            text: "将停止当前插件及其任务，等待旧版本退出后选择 v" + versionDialog.version + "。\n不会自动启动新版本；未保存的终端会话可能丢失。"
            color: themeManager.textPrimary; wrapMode: Text.Wrap; font.pixelSize: 12
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            ToolButton { text: "取消"; onClicked: versionDialog.reject() }
            ToolButton { text: "停止并切换"; onClicked: versionDialog.accept() }
        }
        onAccepted: pluginRegistry.selectVersion(pluginId, version, true)
    }
    Basic.Dialog {
        id: trustDialog
        property string pluginId: ""
        property string packageFolder: ""
        title: "确认信任本地插件"
        modal: true
        background: Rectangle { radius: 16; color: themeManager.cardSurface; border.color: themeManager.border; border.width: 1 }
        header: Text { text: trustDialog.title; color: themeManager.textPrimary; font.pixelSize: 15; padding: 16 }
        footer: RowLayout {
            spacing: 10
            Item { Layout.fillWidth: true }
            ToolButton { text: "取消"; onClicked: trustDialog.reject() }
            ToolButton { text: "确认信任"; onClicked: trustDialog.accept() }
        }
        width: Math.min(440, root.width - 24)
        x: (root.width-width)/2; y: Math.max(12,(root.height-height)/2)
        contentItem: Text {
            text: "插件将以当前用户权限在独立进程运行，不是安全沙盒。仅导入或启用你确认可信的包；不会自动执行，仍需点击打开。\n\n" + (trustDialog.packageFolder || trustDialog.pluginId)
            color: themeManager.textPrimary; wrapMode: Text.Wrap; font.pixelSize: 12
        }
        onAccepted: packageFolder ? pluginRegistry.importTrustedFolder(packageFolder) : pluginRegistry.trustAndEnable(pluginId)
    }
    function inputRegions(target) {
        let items = [heading, status, importCard, footer]
        for (let i = 0; i < plugins.count; ++i) items.push(plugins.itemAt(i))
        return Regions.collect(items, target, viewport)
    }

    Flickable {
        id: viewport
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: parent.width
            spacing: 16

            CardSurface {
                id: heading
                Layout.fillWidth: true; Layout.preferredHeight: 76
                Column {
                    x: 16; y: 17; spacing: 6
                    Text { text: "工具箱"; color: themeManager.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text { text: "每个插件只选择一个活动版本 · 切换不自动启动"; color: themeManager.textMuted; font.pixelSize: 10 }
                }
                Rectangle {
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: 72; height: 32
                    radius: 10
                    color: reloadHover.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong
                    border.width: 1
                    border.color: themeManager.border
                    Text { anchors.centerIn: parent; text: "重新扫描"; color: themeManager.textPrimary; font.pixelSize: 10 }
                    HoverHandler { id: reloadHover }
                    TapHandler { onTapped: pluginRegistry.reload() }
                }
            }

            Rectangle {
                id: status
                Layout.fillWidth: true
                Layout.preferredHeight: statusColumn.implicitHeight + 24
                radius: 14
                color: themeManager.cardSurface
                border.width: 1
                border.color: themeManager.border
                ColumnLayout {
                    id: statusColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 3
                    Text { text: pluginRegistry.statusText; color: themeManager.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Text {
                        Layout.fillWidth: true
                        text: pluginRegistry.errorDetails.length > 0
                              ? pluginRegistry.errorDetails
                              : "独立可选包 · 按需启动 · 不加载外部 QML"
                        color: pluginRegistry.errorDetails.length > 0 ? themeManager.danger : themeManager.textMuted
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
                    }
                }
            }

            CardSurface {
                id: importCard
                Layout.fillWidth: true; Layout.preferredHeight: 108
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 10
                    Text { text: "可信本地插件"; color: themeManager.textPrimary; font.pixelSize: 12 }
                    RowLayout {
                        ToolButton { text: "导入插件包"; onClicked: folderDialog.open() }
                        ToolButton { text: "打开插件目录"; onClicked: pluginRegistry.openPluginFolder() }
                    }
                }
            }
            Repeater {
                id: plugins
                model: pluginRegistry.plugins
                delegate: Rectangle {
                    required property var modelData
                    property alias versionPopup: versionChoice.popup
                    Layout.fillWidth: true
                    Layout.preferredHeight: pluginColumn.implicitHeight + 24
                    radius: 14
                    color: themeManager.cardSurface
                    border.width: 1
                    border.color: themeManager.border

                    ColumnLayout {
                        id: pluginColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name + (modelData.version ? "  v" + modelData.version : " · 未选择版本")
                                    color: themeManager.textPrimary
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.pageCount + " 个入口 · " + (modelData.enabled ? "已启用" : "已停用") + " · " + root.statusLabel(modelData.processStatus)
                                    color: themeManager.textMuted
                                    font.pixelSize: 9
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 58
                                Layout.preferredHeight: 28
                                radius: 10
                                color: modelData.enabled ? themeManager.accentSoft : themeManager.overlayStart
                                border.width: 1
                                border.color: modelData.enabled ? themeManager.accent : themeManager.border
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.enabled ? "停用" : "启用"
                                    color: modelData.enabled ? themeManager.accent : themeManager.textSecondary
                                    font.pixelSize: 10
                                }
                                TapHandler {
                                    enabled: !modelData.pendingVersion && !!modelData.version
                                    onTapped: {
                                        if (!modelData.trusted) { trustDialog.pluginId = modelData.id; trustDialog.packageFolder = ""; trustDialog.open() }
                                        else pluginRegistry.setPluginEnabled(modelData.id, !modelData.enabled)
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.permissionsText
                            color: themeManager.textFaint
                            font.pixelSize: 9
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "活动版本"; color: themeManager.textSecondary; font.pixelSize: 11 }
                            Basic.ComboBox {
                                id: versionChoice
                                objectName: "pluginVersionChoice"
                                Layout.preferredWidth: 180
                                model: modelData.versions || []
                                textRole: "version"
                                currentIndex: (modelData.versions || []).findIndex(v => v.version === modelData.version)
                                enabled: !modelData.pendingVersion
                                indicator: Canvas {
                                    x: parent.width-width-10; y: (parent.height-height)/2; width: 10; height: 6
                                    property color ink: themeManager.textSecondary
                                    onInkChanged: requestPaint()
                                    onPaint: { const c=getContext("2d"); c.clearRect(0,0,width,height); c.fillStyle=ink; c.beginPath(); c.moveTo(0,0); c.lineTo(width,0); c.lineTo(width/2,height); c.closePath(); c.fill() }
                                }
                                contentItem: Text { text: versionChoice.currentIndex < 0 ? "请选择版本" : "v" + versionChoice.currentText; color: themeManager.textPrimary; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 11 }
                                background: Rectangle { radius: 10; color: themeManager.surfaceStrong; border.color: versionChoice.activeFocus ? themeManager.accent : themeManager.border; border.width: 1 }
                                delegate: Basic.ItemDelegate {
                                    required property var modelData
                                    objectName: "pluginVersionOption-" + modelData.version
                                    width: versionChoice.width
                                    contentItem: Text { text: "v" + modelData.version + (modelData.trusted ? " · 已信任" : " · 待信任"); color: themeManager.textPrimary; font.pixelSize: 11 }
                                    background: Rectangle { color: parent.hovered ? themeManager.accentSoft : themeManager.surface }
                                }
                                popup: Basic.Popup {
                                    onOpened: root.versionPopupOpen = true
                                    onClosed: root.versionPopupOpen = false
                                    y: versionChoice.height + 4; width: versionChoice.width; padding: 2
                                    implicitHeight: Math.min(180, versionList.contentHeight + 4)
                                    contentItem: ListView { id: versionList; clip: true; model: versionChoice.popup.visible ? versionChoice.delegateModel : null }
                                    background: Rectangle { radius: 10; color: themeManager.surface; border.color: themeManager.border; border.width: 1 }
                                }
                                onActivated: {
                                    const version = currentText
                                    currentIndex = Qt.binding(() => (modelData.versions || []).findIndex(v => v.version === modelData.version))
                                    if (version === modelData.version) return
                                    if (modelData.running) { versionDialog.pluginId = modelData.id; versionDialog.version = version; versionDialog.open() }
                                    else pluginRegistry.selectVersion(modelData.id, version)
                                }
                            }
                            Text { Layout.fillWidth: true; text: modelData.pendingVersion ? "等待退出 → v" + modelData.pendingVersion : "保留其他版本，不同时运行"; color: themeManager.textMuted; font.pixelSize: 10; wrapMode: Text.Wrap }
                        }
                        RowLayout {
                            ToolButton { text: "打开插件"; enabled: modelData.enabled && !modelData.pendingVersion && modelData.processStatus !== "stopping"; onClicked: pluginRegistry.launch(modelData.id) }
                            ToolButton { text: "停止插件"; enabled: !!modelData.running; onClicked: pluginRegistry.stop(modelData.id) }
                        }
                    }
                }
            }

            CardSurface {
                id: footer
                Layout.fillWidth: true; Layout.preferredHeight: guidance.implicitHeight + 32
                Text {
                    id: guidance
                    anchors.fill: parent; anchors.margins: 16
                    text: (plugins.count === 0 ? "主应用可独立运行，无需安装插件。\n\n" : "") + "插件在单独进程与窗口运行，不共享宿主 QML 引擎。仅使用可信本地包；权限声明不是系统强制隔离。旧共享引擎插件不兼容，请重新导入独立包。"
                    color: themeManager.textMuted; font.pixelSize: 10; wrapMode: Text.Wrap
                }
            }
        }
    }
    component ToolButton: Basic.Button {
        implicitWidth: 112; implicitHeight: 34; opacity: enabled ? 1 : 0.45
        contentItem: Text { text: parent.text; color: themeManager.textPrimary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: parent.down ? themeManager.accentSoft : parent.hovered ? themeManager.surfaceHover : themeManager.surfaceStrong; border.color: parent.activeFocus ? themeManager.accent : themeManager.border; border.width: 1 }
    }
}
