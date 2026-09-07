import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root
    contentWidth: width
    contentHeight: contentColumn.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }

    ColumnLayout {
        id: contentColumn
        width: root.width
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "系统监控"; color: themeManager.textPrimary; font.pixelSize: 18; font.weight: Font.DemiBold }
            Rectangle { Layout.preferredWidth: 6; Layout.preferredHeight: 6; radius: 3; color: themeManager.success }
            Text { text: "实时"; color: themeManager.success; font.pixelSize: 10 }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 118 * Math.ceil((2 + Math.max(1, gpuMonitor.adapters.length)) / 3)
            columns: 3
            rowSpacing: 8
            columnSpacing: 8
            SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "CPU"; value: cpuMonitor.usage.toFixed(0) + "%"; progress: cpuMonitor.usage / 100; accent: themeManager.accent }
            SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "内存"; value: memoryMonitor.usage.toFixed(0) + "%"; progress: memoryMonitor.usage / 100; accent: themeManager.accentSecondary }
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 88
            radius: 16
            color: themeManager.surface
            border.width: 1
            border.color: themeManager.border

            Item {
                anchors.fill: parent
                anchors.margins: 15

                Column {
                    anchors.left: parent.left
                    anchors.right: centerDivider.left
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    Text { width: parent.width; text: "↓  下载"; color: themeManager.textMuted; font.pixelSize: 11 }
                    Text { width: parent.width; text: networkMonitor.downloadSpeedText; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Rectangle { width: parent.width; height: 3; radius: 2; color: themeManager.accentSoft }
                }

                Rectangle {
                    id: centerDivider
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 7
                    anchors.bottomMargin: 7
                    width: 1
                    color: themeManager.track
                }

                Column {
                    anchors.left: centerDivider.right
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    Text { width: parent.width; text: "↑  上传"; color: themeManager.textMuted; font.pixelSize: 11 }
                    Text { width: parent.width; text: networkMonitor.uploadSpeedText; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Rectangle { width: parent.width; height: 3; radius: 2; color: themeManager.accentSecondary }
                }
            }
        }

        DiskPanel { Layout.fillWidth: true; Layout.preferredHeight: implicitHeight; model: diskMonitor }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Text { Layout.fillWidth: true; text: "系统控制"; color: themeManager.textPrimary; font.pixelSize: 16; font.weight: Font.DemiBold }
            Text { text: "展开调节"; color: themeManager.textFaint; font.pixelSize: 9 }
        }

        QuickSettingsPanel { Layout.fillWidth: true }
    }
}
