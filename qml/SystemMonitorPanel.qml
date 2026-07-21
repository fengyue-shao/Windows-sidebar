import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root
    contentWidth: width
    contentHeight: contentColumn.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: contentColumn
        width: root.width
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "系统监控"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
            Text { text: "实时"; color: themeManager.success; font.pixelSize: 10 }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            spacing: 8
            SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "CPU"; value: cpuMonitor.usage.toFixed(0) + "%"; progress: cpuMonitor.usage / 100; accent: themeManager.accent }
            SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "内存"; value: memoryMonitor.usage.toFixed(0) + "%"; progress: memoryMonitor.usage / 100; accent: themeManager.accentSecondary }
            SystemMetricGauge { Layout.fillWidth: true; Layout.fillHeight: true; label: "GPU"; value: gpuMonitor.usage.toFixed(0) + "%"; progress: gpuMonitor.usage / 100; accent: themeManager.warning }
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
    }
}
