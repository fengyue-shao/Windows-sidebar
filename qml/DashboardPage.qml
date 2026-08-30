import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool mediaEnabled: true
    property bool monitorEnabled: true
    property bool notificationsEnabled: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        MediaBar {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 152 : 0
            moduleEnabled: root.mediaEnabled
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: root.monitorEnabled

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: "系统监控"
                    color: themeManager.textPrimary
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: themeManager.success
                }
                Text { text: "实时"; color: themeManager.success; font.pixelSize: 10 }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 116 * Math.ceil((2 + Math.max(1, gpuMonitor.adapters.length)) / 3)
                columns: 3
                rowSpacing: 8
                columnSpacing: 8

                SystemMetricGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "CPU"
                    value: cpuMonitor.usage.toFixed(0) + "%"
                    progress: cpuMonitor.usage / 100
                    accent: themeManager.accent
                }
                SystemMetricGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "内存"
                    value: memoryMonitor.usage.toFixed(0) + "%"
                    progress: memoryMonitor.usage / 100
                    accent: themeManager.accentSecondary
                }
                SystemMetricGauge {
                    visible: gpuMonitor.adapters.length === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: "GPU"
                    value: gpuMonitor.usage.toFixed(0) + "%"
                    progress: gpuMonitor.usage / 100
                    accent: themeManager.warning
                }
                Repeater {
                    model: gpuMonitor.adapters
                    delegate: SystemMetricGauge {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        label: modelData.name
                        detail: modelData.description || "图形处理器"
                        value: Number(modelData.usage).toFixed(0) + "%"
                        progress: Number(modelData.usage) / 100
                        accent: themeManager.warning
                    }
                }
            }
        }

        NotificationArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: visible ? 178 : 0
            visible: root.notificationsEnabled
            model: notificationModel
        }

        Item { Layout.fillHeight: !root.notificationsEnabled }
    }
}
