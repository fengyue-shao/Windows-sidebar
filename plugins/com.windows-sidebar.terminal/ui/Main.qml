import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text { text: "受限终端"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text { text: terminalManager.statusText; color: terminalManager.running ? themeManager.warning : themeManager.textMuted; font.pixelSize: 9 }
            }
            Rectangle {
                Layout.preferredWidth: 72; Layout.preferredHeight: 26; radius: 10
                color: themeManager.overlayStart; border.width: 1; border.color: themeManager.warning
                Text { anchors.centerIn: parent; text: "实验性隔离"; color: themeManager.warning; font.pixelSize: 8; font.weight: Font.DemiBold }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 48; radius: 13
            color: themeManager.surface; border.width: 1; border.color: themeManager.border
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 11; anchors.rightMargin: 11; spacing: 7
                Text { text: "▣"; color: themeManager.accent; font.pixelSize: 10 }
                Text { Layout.fillWidth: true; text: terminalManager.sandboxPath; color: themeManager.textMuted; font.pixelSize: 8; elide: Text.ElideMiddle }
                Text { text: "UTF-8 · " + terminalManager.shellName; color: themeManager.textFaint; font.pixelSize: 8 }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; radius: 16
            color: themeManager.dark ? "#0E1117" : "#F4F6F9"
            border.width: 1; border.color: themeManager.border
            clip: true
            ScrollView {
                anchors.fill: parent; anchors.margins: 10
                TextArea {
                    id: terminalOutput
                    text: terminalManager.output.length > 0 ? terminalManager.output : "Windows Sidebar Restricted Runner\nShell: " + terminalManager.shellName + "\nEncoding: UTF-8\n输入命令后按 Enter 执行。\n"
                    readOnly: true
                    textFormat: TextEdit.PlainText
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: themeManager.dark ? "#D7E0EA" : "#253040"
                    font.family: "Cascadia Mono"
                    font.pixelSize: 10
                    background: null
                    onTextChanged: cursorPosition = length
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; spacing: 7
            TextField {
                id: commandInput
                Layout.fillWidth: true; implicitHeight: 42
                placeholderText: "例如：Get-ChildItem 或 Get-Date"
                enabled: !terminalManager.running
                selectByMouse: true
                font.family: "Cascadia Mono"
                onAccepted: {
                    terminalManager.run(text)
                    if (text.trim().length > 0) text = ""
                }
            }
            ActionButton {
                label: terminalManager.running ? "停止" : "运行"
                danger: terminalManager.running
                onClicked: {
                    if (terminalManager.running) terminalManager.stop()
                    else { terminalManager.run(commandInput.text); commandInput.text = "" }
                }
            }
            ActionButton { label: "清屏"; onClicked: terminalManager.clear() }
            ActionButton { label: "日志"; onClicked: terminalManager.openLogFile() }
        }

        Text {
            Layout.fillWidth: true
            text: "限制：Constrained Language · 无配置文件 · 固定初始目录 · 60 秒超时 · 256 MB/进程 · 最多 6 个进程。当前不是 AppContainer，不能作为不可信代码的强安全边界。"
            color: themeManager.textFaint; font.pixelSize: 8; wrapMode: Text.WordWrap
        }
    }

    component ActionButton: Rectangle {
        id: action
        property string label
        property bool danger: false
        signal clicked()
        implicitWidth: 58; implicitHeight: 42; radius: 12
        color: actionHover.hovered ? (danger ? themeManager.dangerSoft : themeManager.accentSoft) : themeManager.surface
        border.width: 1; border.color: danger ? themeManager.danger : themeManager.border
        Text { anchors.centerIn: parent; text: action.label; color: action.danger ? themeManager.danger : themeManager.accent; font.pixelSize: 9; font.weight: Font.DemiBold }
        HoverHandler { id: actionHover }
        TapHandler { onTapped: action.clicked() }
    }
}
