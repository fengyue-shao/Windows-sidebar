import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var commandHistory: []
    property int historyIndex: commandHistory.length
    property int paletteIndex: 0
    property int reverseSearchIndex: -1
    property string reverseSearchQuery: ""
    property string notice: ""
    property var slashCommands: [
        { name: "/help", description: "显示快捷键与内置命令" },
        { name: "/status", description: "查看当前会话与隔离状态" },
        { name: "/clear", description: "清空终端显示（Ctrl+L）" },
        { name: "/history", description: "显示本次会话的命令历史" },
        { name: "/logs", description: "打开插件调试日志" },
        { name: "/stop", description: "中止正在运行的命令" },
        { name: "/reset", description: "清空显示与命令历史" }
    ]

    function filteredCommands() {
        const query = commandInput.text.toLowerCase()
        return slashCommands.filter(item => item.name.indexOf(query) === 0)
    }

    function completionItems() {
        if (commandInput.text.startsWith("/"))
            return filteredCommands()
        const raw = terminalManager.completionSuggestions(commandInput.text)
        const items = []
        for (let index = 0; index < raw.length; ++index) {
            const parts = raw[index].split("\t")
            items.push({ name: parts[0], kind: parts[1], description: parts[2] })
        }
        return items
    }

    function acceptCompletion(item) {
        if (!item) return
        if (commandInput.text.startsWith("/")) {
            commandInput.text = item.name
        } else {
            const space = Math.max(commandInput.text.lastIndexOf(" "), commandInput.text.lastIndexOf("\t"))
            commandInput.text = commandInput.text.slice(0, space + 1) + item.name
        }
        commandInput.cursorPosition = commandInput.text.length
        commandInput.forceActiveFocus()
    }

    function completeCommand() {
        if (!commandInput.activeFocus)
            return
        const matches = completionItems()
        if (commandPalette.visible && matches.length > 0) {
            acceptCompletion(matches[Math.min(paletteIndex, matches.length - 1)])
        }
        commandInput.forceActiveFocus()
    }

    Shortcut {
        sequence: "Tab"
        context: Qt.WindowShortcut
        enabled: commandInput.activeFocus
        onActivated: root.completeCommand()
    }

    Shortcut {
        sequence: "Shift+Tab"
        context: Qt.WindowShortcut
        enabled: commandInput.activeFocus
        onActivated: commandInput.forceActiveFocus()
    }

    function runSlashCommand(command) {
        notice = ""
        switch (command) {
        case "/help":
            notice = "Enter 执行 · Tab 补全 · ↑↓ 历史/选择 · Ctrl+C 或 Esc 中止 · Ctrl+L 清屏"
            break
        case "/status":
            notice = "就绪 · " + terminalManager.shellName + " · " + terminalManager.transportName + " · UTF-8 · 会话变量 " + terminalManager.sessionEnvironmentCount + " · 当前目录：" + terminalManager.currentDirectory
            break
        case "/clear": terminalManager.clear(); break
        case "/history":
            notice = commandHistory.length > 0 ? commandHistory.join("  ·  ") : "本次会话还没有命令历史"
            break
        case "/logs": terminalManager.openLogFile(); notice = "已请求打开终端插件日志"; break
        case "/stop": terminalManager.stop(); break
        case "/reset":
            terminalManager.clear(); commandHistory = []; historyIndex = 0
            notice = "会话显示与输入历史已重置"
            break
        default: notice = "未知内置命令。输入 / 查看可用命令"; break
        }
    }

    function reverseSearchHistory() {
        if (commandHistory.length === 0) {
            notice = "反向搜索：暂无历史命令"
            return
        }
        if (reverseSearchIndex < 0) {
            reverseSearchQuery = commandInput.text
            reverseSearchIndex = commandHistory.length
        }
        for (let index = reverseSearchIndex - 1; index >= 0; --index) {
            if (!reverseSearchQuery || commandHistory[index].toLowerCase().indexOf(reverseSearchQuery.toLowerCase()) >= 0) {
                reverseSearchIndex = index
                commandInput.text = commandHistory[index]
                commandInput.cursorPosition = commandInput.text.length
                notice = "反向搜索（Ctrl+R 继续）：" + (reverseSearchQuery || "全部历史")
                return
            }
        }
        reverseSearchIndex = commandHistory.length
        notice = "已到达历史起点；再次按 Ctrl+R 从最新记录继续"
    }

    function submitCommand() {
        const command = commandInput.text.trim()
        if (!command || terminalManager.running) return
        if (command.startsWith("/")) {
            const matches = filteredCommands()
            const selected = matches.length > 0 ? matches[Math.min(paletteIndex, matches.length - 1)].name : command
            runSlashCommand(selected)
            commandInput.clear()
            paletteIndex = 0
            return
        }
        if (commandHistory.length === 0 || commandHistory[commandHistory.length - 1] !== command)
            commandHistory.push(command)
        if (commandHistory.length > 100) commandHistory.shift()
        historyIndex = commandHistory.length
        terminalManager.run(command)
        notice = ""
        reverseSearchIndex = -1
        reverseSearchQuery = ""
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
                Text { Layout.fillWidth: true; text: terminalManager.currentDirectory; color: themeManager.textMuted; font.pixelSize: 8; elide: Text.ElideMiddle }
                Text { text: "UTF-8 · ENV " + terminalManager.sessionEnvironmentCount + " · " + terminalManager.transportName; color: themeManager.textFaint; font.pixelSize: 8 }
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
            Rectangle {
                id: commandPalette
                visible: commandInput.text.length > 0 && root.completionItems().length > 0 && !terminalManager.running
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: promptRow.top
                anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.bottomMargin: 6
                height: Math.min(210, paletteColumn.implicitHeight + 12)
                radius: 12; z: 4
                color: themeManager.dark ? "#171C24" : "#FFFFFF"
                border.width: 1; border.color: themeManager.accent
                clip: true
                Column {
                    id: paletteColumn
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 6; spacing: 2
                    Repeater {
                        model: root.completionItems()
                        Rectangle {
                            required property var modelData
                            required property int index
                            width: paletteColumn.width; height: 28; radius: 7
                            color: index === root.paletteIndex ? themeManager.accentSoft : "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                Text { text: modelData.name; color: themeManager.accent; font.family: "Cascadia Mono"; font.pixelSize: 10; font.weight: Font.DemiBold }
                                Rectangle {
                                    visible: modelData.kind !== undefined
                                    implicitWidth: kindLabel.implicitWidth + 10; implicitHeight: 18; radius: 6
                                    color: themeManager.overlayStart
                                    Text { id: kindLabel; anchors.centerIn: parent; text: modelData.kind || ""; color: themeManager.textFaint; font.pixelSize: 8 }
                                }
                                Text { Layout.fillWidth: true; text: modelData.description; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
                            }
                            TapHandler { onTapped: root.acceptCompletion(modelData) }
                        }
                    }
                }
            }
            Rectangle {
                visible: root.notice.length > 0 && !commandPalette.visible
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: promptRow.top
                anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.bottomMargin: 6
                height: noticeText.implicitHeight + 16; radius: 10; z: 3
                color: themeManager.dark ? "#171C24" : "#FFFFFF"
                border.width: 1; border.color: themeManager.border
                Text {
                    id: noticeText
                    anchors.fill: parent; anchors.margins: 8
                    text: root.notice; color: themeManager.textMuted; font.pixelSize: 9
                    wrapMode: Text.WrapAnywhere
                }
                TapHandler { onTapped: root.notice = "" }
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
                activeFocusOnTab: false
                KeyNavigation.tab: commandInput
                KeyNavigation.backtab: commandInput
                Keys.onTabPressed: event => { root.completeCommand(); event.accepted = true }
                Keys.onBacktabPressed: event => { forceActiveFocus(); event.accepted = true }
                Keys.onReturnPressed: event => { root.submitCommand(); event.accepted = true }
                Keys.onEnterPressed: event => { root.submitCommand(); event.accepted = true }
                onTextChanged: root.paletteIndex = 0
                Keys.onUpPressed: event => {
                    if (commandPalette.visible) {
                        root.paletteIndex = Math.max(0, root.paletteIndex - 1)
                        event.accepted = true
                        return
                    }
                    if (root.commandHistory.length > 0) {
                        root.historyIndex = Math.max(0, root.historyIndex - 1)
                        text = root.commandHistory[root.historyIndex]
                        cursorPosition = text.length
                    }
                    event.accepted = true
                }
                Keys.onDownPressed: event => {
                    if (commandPalette.visible) {
                        root.paletteIndex = Math.min(root.completionItems().length - 1, root.paletteIndex + 1)
                        event.accepted = true
                        return
                    }
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
                    if ((event.key === Qt.Key_Escape || ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C)) && terminalManager.running) {
                        terminalManager.stop(); event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        clear(); root.notice = ""; root.reverseSearchIndex = -1; event.accepted = true
                    } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_L) {
                        terminalManager.clear(); event.accepted = true
                    } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_R) {
                        root.reverseSearchHistory(); event.accepted = true
                    }
                }
                Component.onCompleted: forceActiveFocus()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "输入 / 打开命令面板 · Tab 补全 · Ctrl+R 搜索历史 · Esc/Ctrl+C 停止。限制：Constrained Language · 60 秒超时。"
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
