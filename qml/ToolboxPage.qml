import QtQuick
import QtQuick.Layouts
import "CardRegions.js" as Regions

Item {
    id: root
    function inputRegions(target) {
        let items = [heading, status, footer]
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
                              : "插件从应用目录的 plugins 文件夹加载"
                        color: pluginRegistry.errorDetails.length > 0 ? themeManager.danger : themeManager.textMuted
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
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
                                    text: modelData.pageCount + " 个页面 · " + (modelData.enabled ? "已启用" : "已停用")
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
                                    onTapped: pluginRegistry.setPluginEnabled(modelData.id, !modelData.enabled)
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
                    }
                }
            }

            CardSurface {
                id: footer
                Layout.fillWidth: true; Layout.preferredHeight: guidance.implicitHeight + 32
                Text {
                    id: guidance
                    anchors.fill: parent; anchors.margins: 16
                    text: (plugins.count === 0 ? "暂无插件，请检查插件目录后重新扫描。\n\n" : "") + "只加载你信任的插件。插件页目前共享宿主 QML 引擎，权限清单尚未形成强制隔离。"
                    color: themeManager.textMuted; font.pixelSize: 10; wrapMode: Text.Wrap
                }
            }
        }
    }
}
