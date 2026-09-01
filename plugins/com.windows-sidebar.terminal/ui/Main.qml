import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var commandHistory: []
    property int historyIndex: commandHistory.length

    function submitCommand() {
        const command = commandInput.text.trim()
        if (!command || terminalManager.running) return
        if (commandHistory.length === 0 || commandHistory[commandHistory.length - 1] !== command)
            commandHistory.push(command)
        if (commandHistory.length > 100) commandHistory.shift()
        historyIndex = commandHistory.length
        terminalManager.run(command)
        commandInput.clear()
    }

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
            ActionButton { visible: terminalManager.running; label: "停止"; danger: true; onClicked: terminalManager.stop() }
            ActionButton { label: "日志"; onClicked: terminalManager.openLogFile() }
            ActionButton { label: "清屏"; onClicked: terminalManager.clear() }
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
            TapHandler { onTapped: commandInput.forceActiveFocus() }
            ScrollView {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: promptRow.top
                anchors.margins: 10; anchors.bottomMargin: 4
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
            RowLayout {
                id: promptRow
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: 8
                height: 30; spacing: 7
                Text {
                    text: terminalManager.running ? "●" : "PS sandbox>"
                    color: terminalManager.running ? themeManager.warning : themeManager.success
                    font.family: "Cascadia Mono"; font.pixelSize: 10; font.weight: Font.DemiBold
                }
                TextInput {
                id: commandInput
                Layout.fillWidth: true
                enabled: !terminalManager.running
                selectByMouse: true
                color: themeManager.dark ? "#E6EDF3" : "#1F2937"
                selectionColor: themeManager.accent
                font.family: "Cascadia Mono"
                font.pixelSize: 10
                Keys.onReturnPressed: event => { root.submitCommand(); event.accepted = true }
                Keys.onEnterPressed: event => { root.submitCommand(); event.accepted = true }
                Keys.onUpPressed: event => {
                    if (root.commandHistory.length > 0) {
                        root.historyIndex = Math.max(0, root.historyIndex - 1)
                        text = root.commandHistory[root.historyIndex]
                        cursorPosition = text.length
                    }
                    event.accepted = true
                }
                Keys.onDownPressed: event => {
                    if (root.historyIndex < root.commandHistory.length - 1) {
                        root.historyIndex++
                        text = root.commandHistory[root.historyIndex]
                    } else {
                        root.historyIndex = root.commandHistory.length
                        text = ""
                    }
                    cursorPosition = text.length
                    event.accepted = true
                }
                Keys.onPressed: event => {
                    if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C && terminalManager.running) {
                        terminalManager.stop(); event.accepted = true
                    } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_L) {
                        terminalManager.clear(); event.accepted = true
                    }
                }
                Component.onCompleted: forceActiveFocus()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "直接点击终端输入 · ↑↓ 历史 · Ctrl+C 停止 · Ctrl+L 清屏。限制：Constrained Language · 60 秒超时 · 256 MB/进程 · 最多 6 个进程。"
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
