import QtQuick
import QtQuick.Layouts

Item {
    id: root

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: parent.width
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: "工具箱"; color: themeManager.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text { text: "在这里管理侧栏插件"; color: themeManager.textMuted; font.pixelSize: 10 }
                }
                Rectangle {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 30
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
                Layout.fillWidth: true
                Layout.preferredHeight: statusColumn.implicitHeight + 24
                radius: 14
                color: themeManager.surface
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

            Text { text: "已发现的插件"; color: themeManager.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold }

            Repeater {
                model: pluginRegistry.plugins
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: pluginColumn.implicitHeight + 24
                    radius: 14
                    color: themeManager.surface
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

            Text {
                visible: pluginRegistry.plugins.length === 0
                Layout.fillWidth: true
                text: "还没有可管理的插件。请检查插件目录，然后重新扫描。"
                color: themeManager.textMuted
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                text: "插件页目前与宿主共享 QML 引擎；上方权限仅为清单声明，尚未形成强制隔离。请只加载你信任的插件。"
                color: themeManager.textFaint
                font.pixelSize: 9
                wrapMode: Text.Wrap
            }
        }
    }
}
