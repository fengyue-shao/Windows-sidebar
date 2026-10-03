import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import "CardRegions.js" as Regions

Item {
    id: root
    readonly property bool popupOpen: trustDialog.visible || folderDialog.visible
    function closePopups() { trustDialog.close(); folderDialog.close() }
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
                    Text { text: "在这里管理侧栏插件"; color: themeManager.textMuted; font.pixelSize: 10 }
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
                                    text: modelData.name + "  v" + modelData.version
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
                            ToolButton { text: "打开插件"; enabled: modelData.enabled && modelData.processStatus !== "stopping"; onClicked: pluginRegistry.launch(modelData.id) }
                            ToolButton { text: "停止插件"; enabled: ["starting","running"].indexOf(modelData.processStatus)>=0; onClicked: pluginRegistry.stop(modelData.id) }
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
