import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WindowsSidebar.Terminal 1.0

Item {
    id: root
    objectName: "terminalPlugin"
    property int islandRunPriority: 10
    property int islandDonePriority: 80
    property int islandErrorPriority: 90
    property int islandEventSerial: 0
    readonly property bool nativeMode: terminalManager.transportInteractive || (!terminalManager.sessionActive && terminalManager.preferredTransport !== "pipe")
    readonly property var screen: terminalManager.screen
    readonly property real cellWidth: Math.max(1, terminalMetrics.advanceWidth("M"))
    readonly property real cellHeight: Math.max(1, Math.ceil(terminalMetrics.height))
    property bool assistantVisible: false
    property bool copyMode: false
    property bool followOutput: true
    property bool flushingInput: false
    property string pendingPaste: ""
    property string notice: ""
    property var commandHistory: []
    property int historyIndex: commandHistory.length
    property string historyDraft: ""
    property int paletteIndex: 0
    property var candidates: []
    property int reverseSearchIndex: -1
    property string reverseSearchQuery: ""
    property var slashCommands: [
        { name: "/help", description: "查看原生输入与辅助输入快捷键", kind: "内置" },
        { name: "/status", description: "查看 Shell、传输与限制", kind: "内置" },
        { name: "/clear", description: "清空本地终端显示", kind: "内置" },
        { name: "/history", description: "查看辅助输入命令历史", kind: "内置" },
        { name: "/logs", description: "打开插件日志", kind: "内置" },
        { name: "/stop", description: "结束 Shell 与所属进程树", kind: "内置" },
        { name: "/reset", description: "重置 Shell、环境、目录和输入历史", kind: "内置" }
    ]
    FontMetrics { id: terminalMetrics; font.family: Qt.fontFamilies().indexOf("Cascadia Mono") >= 0 ? "Cascadia Mono" : "Consolas"; font.pixelSize: 12; font.kerning: false; font.features: { "liga": 0, "clig": 0 } }

    function formatElapsed(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + seconds % 60
    }
    function publishActivity() {
        if (typeof islandBridge === "undefined") return
        if (terminalManager.running || (terminalManager.transportInteractive && terminalManager.sessionActive)) {
            islandBridge.publishState("plugin.terminal.running", {
                title: terminalManager.running ? "终端任务执行中" : "交互终端已打开",
                subtitle: terminalManager.shellName + " · " + formatElapsed(terminalManager.elapsedMilliseconds),
                iconName: "terminal", route: "terminal", kind: "terminal"
            }, islandRunPriority)
        } else islandBridge.withdraw("plugin.terminal.running")
    }
    function publishOutcome(kind, outcome, exitCode, elapsedMs) {
        if (typeof islandBridge === "undefined" || outcome === "reset") return
        const names = { success: "已完成", failed: "未成功", cancelled: "已停止", timeout: "已超时", crashed: "异常退出", "start-failed": "启动失败" }
        const failed = outcome === "failed" || outcome === "crashed" || outcome === "start-failed"
        const warning = outcome === "cancelled" || outcome === "timeout"
        islandBridge.pushEvent("plugin.terminal", "result-" + Date.now() + "-" + (++islandEventSerial), {
            title: (kind === "session" ? "终端会话" : "终端任务") + (names[outcome] || "已结束"),
            subtitle: "耗时 " + (elapsedMs / 1000).toFixed(1) + " 秒" + ((outcome === "success" || outcome === "failed" || outcome === "crashed") ? " · 退出码 " + exitCode : ""),
            iconName: failed ? "terminal" : "checkmark", route: "terminal", kind: "terminal", tone: failed ? "danger" : warning ? "warning" : "success"
        }, failed || outcome === "timeout" ? islandErrorPriority : islandDonePriority, 4000, 15000)
    }
    Connections {
        target: terminalManager
        function onRunningChanged() { root.publishActivity() }
        function onSessionChanged() { root.publishActivity(); root.copyMode = false; root.scheduleResize() }
        function onTransportChanged() { root.scheduleResize() }
        function onOperationFinished(kind, outcome, exitCode, elapsedMs) { root.publishOutcome(kind, outcome, exitCode, elapsedMs) }
    }
    Connections { target: root.screen; function onChanged() { if (root.followOutput && !root.copyMode) Qt.callLater(root.scrollToBottom) } }
    Timer { interval: 1000; repeat: true; running: terminalManager.running || (terminalManager.transportInteractive && terminalManager.sessionActive); onTriggered: root.publishActivity() }
    Component.onDestruction: {
        if (typeof islandBridge !== "undefined") { islandBridge.withdraw("plugin.terminal.running"); islandBridge.withdraw("plugin.terminal") }
    }

    function scrollToBottom() { nativeScroll.contentY = Math.max(0, nativeScroll.contentHeight - nativeScroll.height) }
    function scheduleResize() { resizeDebounce.restart() }
    function resizeGrid() {
        if (nativeScroll.width > 0 && nativeScroll.height > 0)
            terminalManager.resizeTerminal(Math.max(10, Math.floor(nativeScroll.width / cellWidth)), Math.max(2, Math.floor(nativeScroll.height / cellHeight)))
    }
    Timer { id: resizeDebounce; interval: 100; onTriggered: root.resizeGrid() }
    function focusTerminal() {
        if (!nativeMode || assistantVisible) commandInput.forceActiveFocus()
        else if (!copyMode) rawTextInput.forceActiveFocus()
    }
    function sendRaw(value) {
        if (!terminalManager.transportInteractive || !terminalManager.sessionActive || !value) return
        followOutput = true; scrollToBottom(); terminalManager.sendInput(value)
    }
    function flushCommittedInput() {
        if (flushingInput || rawTextInput.inputMethodComposing || rawTextInput.text.length === 0) return
        flushingInput = true; sendRaw(rawTextInput.text); rawTextInput.clear(); flushingInput = false
    }
    function sendPaste(value) {
        if (!value) return
        if (screen.bracketedPaste) sendRaw("\x1b[200~" + value + "\x1b[201~")
        else sendRaw(value)
        rawTextInput.forceActiveFocus()
    }
    function pasteClipboard() {
        clipboardBuffer.clear(); clipboardBuffer.paste()
        const value = clipboardBuffer.text
        clipboardBuffer.clear()
        if (!value) return
        // The foreground program owns paste interpretation; even bracketed paste is not a security boundary.
        if (/[\r\n]/.test(value)) { pendingPaste = value; pasteConfirm.open() }
        else sendPaste(value)
    }
    function copyAllOutput() {
        clipboardBuffer.text = nativeMode ? screen.plainText() : terminalManager.output
        clipboardBuffer.selectAll(); clipboardBuffer.copy(); clipboardBuffer.clear()
        notice = "已复制当前输出（不包含输入历史）"
    }
    function arrowSequence(letter, modifiers) {
        const modifier = 1 + ((modifiers & Qt.ShiftModifier) ? 1 : 0) + ((modifiers & Qt.AltModifier) ? 2 : 0) + ((modifiers & Qt.ControlModifier) ? 4 : 0)
        if (modifier > 1) return "\x1b[1;" + modifier + letter
        return screen.applicationCursorKeys ? "\x1bO" + letter : "\x1b[" + letter
    }
    function nativeKey(event) {
        // Input methods need these keys for candidate navigation/commit; do not intercept during preedit.
        if (rawTextInput.inputMethodComposing) return
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0
        if (ctrl && event.key === Qt.Key_Space) { assistantVisible = !assistantVisible; focusTerminal(); event.accepted = true; return }
        if (ctrl && shift && event.key === Qt.Key_C) { copyAllOutput(); event.accepted = true; return }
        if (ctrl && event.key === Qt.Key_V) { pasteClipboard(); event.accepted = true; return }
        if (ctrl && shift && event.key === Qt.Key_L) { terminalManager.clear(); event.accepted = true; return }
        let value = ""
        switch (event.key) {
        case Qt.Key_Return: case Qt.Key_Enter: value = "\r"; break
        case Qt.Key_Tab: value = shift ? "\x1b[Z" : "\t"; break
        case Qt.Key_Backtab: value = "\x1b[Z"; break
        case Qt.Key_Backspace: value = "\x7f"; break
        case Qt.Key_Escape: value = "\x1b"; break
        case Qt.Key_Up: value = arrowSequence("A", event.modifiers); break
        case Qt.Key_Down: value = arrowSequence("B", event.modifiers); break
        case Qt.Key_Right: value = arrowSequence("C", event.modifiers); break
        case Qt.Key_Left: value = arrowSequence("D", event.modifiers); break
        case Qt.Key_Home: value = "\x1b[H"; break
        case Qt.Key_End: value = "\x1b[F"; break
        case Qt.Key_Insert: value = "\x1b[2~"; break
        case Qt.Key_Delete: value = "\x1b[3~"; break
        case Qt.Key_PageUp: value = "\x1b[5~"; break
        case Qt.Key_PageDown: value = "\x1b[6~"; break
        case Qt.Key_F1: value = "\x1bOP"; break
        case Qt.Key_F2: value = "\x1bOQ"; break
        case Qt.Key_F3: value = "\x1bOR"; break
        case Qt.Key_F4: value = "\x1bOS"; break
        case Qt.Key_F5: value = "\x1b[15~"; break
        case Qt.Key_F6: value = "\x1b[17~"; break
        case Qt.Key_F7: value = "\x1b[18~"; break
        case Qt.Key_F8: value = "\x1b[19~"; break
        case Qt.Key_F9: value = "\x1b[20~"; break
        case Qt.Key_F10: value = "\x1b[21~"; break
        case Qt.Key_F11: value = "\x1b[23~"; break
        case Qt.Key_F12: value = "\x1b[24~"; break
        default:
            if (ctrl && event.key >= Qt.Key_A && event.key <= Qt.Key_Z) value = String.fromCharCode(event.key - Qt.Key_A + 1)
            else if ((event.modifiers & Qt.AltModifier) && !ctrl && event.text) value = "\x1b" + event.text
            break
        }
        if (value) { sendRaw(value); event.accepted = true }
    }

    function refreshCompletions() {
        const query = commandInput.text
        const items = []
        if (query.startsWith("/")) {
            const matches = slashCommands.filter(item => item.name.indexOf(query.toLowerCase()) === 0)
            for (let i = 0; i < matches.length; ++i) items.push(matches[i])
        } else if (query.length > 0) {
            const raw = terminalManager.completionSuggestions(query)
            for (let i = 0; i < raw.length && items.length < 6; ++i) {
                const parts = raw[i].split("\t")
                items.push({ name: parts[0], kind: parts[1] || "候选", description: parts[2] || "" })
            }
        }
        candidates = items.slice(0, 6); paletteIndex = Math.min(paletteIndex, Math.max(0, candidates.length - 1))
    }
    Timer { id: completionDebounce; interval: 120; onTriggered: root.refreshCompletions() }
    function completedDraft(item) {
        if (!item) return commandInput.text
        if (commandInput.text.startsWith("/")) return item.name
        const start = terminalManager.completionStart(commandInput.text)
        return commandInput.text.slice(0, Math.max(0, start)) + item.name
    }
    function acceptCompletion(item) {
        if (!item) return
        commandInput.text = completedDraft(item); commandInput.cursorPosition = commandInput.text.length; commandInput.forceActiveFocus()
    }
    function completeCommand() { refreshCompletions(); if (candidates.length) acceptCompletion(candidates[Math.min(paletteIndex, candidates.length - 1)]); commandInput.forceActiveFocus() }
    function moveCandidate(direction) { refreshCompletions(); if (candidates.length) paletteIndex = (paletteIndex + direction + candidates.length) % candidates.length; commandInput.forceActiveFocus() }
    Shortcut {
        sequence: "Tab"; context: Qt.WindowShortcut
        enabled: (commandInput.activeFocus || rawTextInput.activeFocus) && !rawTextInput.inputMethodComposing && !commandInput.inputMethodComposing
        onActivated: { if (rawTextInput.activeFocus) root.sendRaw("\t"); else root.completeCommand() }
    }
    Shortcut {
        sequence: "Shift+Tab"; context: Qt.WindowShortcut
        enabled: (commandInput.activeFocus || rawTextInput.activeFocus) && !rawTextInput.inputMethodComposing && !commandInput.inputMethodComposing
        onActivated: { if (rawTextInput.activeFocus) root.sendRaw("\x1b[Z"); else root.moveCandidate(-1) }
    }
    function runSlashCommand(command) {
        notice = ""
        switch (command) {
        case "/help": notice = nativeMode ? "原生 Tab 交由 Shell 补全；Ctrl+C 发中断；Esc 发给前台程序。Ctrl+Space 辅助输入；Ctrl+Shift+C 复制输出。辅助框 Enter 只插入，不执行。" : "Enter 执行 · Tab 接受预览 · Shift+Tab 切换 · ↑↓ 历史/选择 · Ctrl+R 搜索 · Ctrl+C/Esc 结束任务 · Ctrl+L 清屏"; break
        case "/status": notice = terminalManager.shellName + " · " + terminalManager.transportName + " · UTF-8 · 变量 " + terminalManager.sessionEnvironmentCount + " · 管道超时 " + (terminalManager.commandTimeoutSeconds === 0 ? "关闭" : terminalManager.commandTimeoutSeconds + " 秒") + " · 原生会话不设空闲超时"; break
        case "/clear": terminalManager.clear(); break
        case "/history": notice = commandHistory.length ? commandHistory.join("  ·  ") : "辅助输入还没有历史；原生历史由 Shell 管理"; break
        case "/logs": terminalManager.openLogFile(); notice = "已请求打开插件日志"; break
        case "/stop": terminalManager.stop(); break
        case "/reset": terminalManager.resetSession(); commandHistory = []; historyIndex = 0; assistantVisible = false; notice = "Shell、环境变量、工作目录和历史已重置"; break
        default: notice = "未知内置命令；输入 / 查看操作"; break
        }
    }
    function rememberCommand(command) {
        const next = commandHistory.slice()
        if (!next.length || next[next.length - 1] !== command) next.push(command)
        if (next.length > 100) next.shift()
        commandHistory = next; historyIndex = next.length; historyDraft = ""
    }
    function submitCommand() {
        if (commandInput.inputMethodComposing) return
        const command = commandInput.text.trim()
        if (!command || (!nativeMode && terminalManager.running)) return
        if (command.startsWith("/")) runSlashCommand(command)
        else if (nativeMode) {
            if (!terminalManager.sessionActive || !terminalManager.transportInteractive) { notice = "请先手动启动原生会话"; return }
            // Never add Enter implicitly: assistant drafts must be reviewed in the terminal before execution.
            sendPaste(commandInput.text); rememberCommand(command); notice = "草稿已插入；检查终端后自行按 Enter 执行"
        } else { rememberCommand(command); terminalManager.run(command); notice = "" }
        reverseSearchIndex = -1; reverseSearchQuery = ""; paletteIndex = 0; commandInput.clear()
        if (nativeMode) { assistantVisible = false; rawTextInput.forceActiveFocus() }
    }
    function reverseSearchHistory() {
        if (!commandHistory.length) { notice = "反向搜索：暂无辅助输入历史"; return }
        if (reverseSearchIndex < 0) { reverseSearchQuery = commandInput.text; reverseSearchIndex = commandHistory.length }
        for (let i = reverseSearchIndex - 1; i >= 0; --i) {
            if (!reverseSearchQuery || commandHistory[i].toLowerCase().indexOf(reverseSearchQuery.toLowerCase()) >= 0) {
                reverseSearchIndex = i; commandInput.text = commandHistory[i]; commandInput.cursorPosition = commandInput.text.length
                notice = "反向搜索（Ctrl+R 继续）：" + (reverseSearchQuery || "全部历史"); return
            }
        }
        reverseSearchIndex = commandHistory.length; notice = "已到历史起点；再次按 Ctrl+R 从最新记录继续"
    }
    function historyStep(direction) {
        if (!commandHistory.length) return
        if (historyIndex === commandHistory.length) historyDraft = commandInput.text
        historyIndex = Math.max(0, Math.min(commandHistory.length, historyIndex + direction))
        commandInput.text = historyIndex === commandHistory.length ? historyDraft : commandHistory[historyIndex]
        commandInput.cursorPosition = commandInput.text.length
    }
    TextEdit { id: clipboardBuffer; visible: false; textFormat: TextEdit.PlainText }
    Dialog {
        id: pasteConfirm; objectName: "terminalPasteConfirmation"; modal: true; title: "确认多行粘贴"; width: Math.min(root.width - 16, 420)
        contentHeight: 112
        anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: { const value = root.pendingPaste; root.pendingPaste = ""; root.sendPaste(value) }
        onRejected: { root.pendingPaste = ""; root.focusTerminal() }
        contentItem: Text {
            text: "文本包含换行，前台程序可能立即执行多条命令。确认剪贴板来源可信后再粘贴。\n" + (root.screen.bracketedPaste ? "当前程序已启用括号粘贴。" : "当前程序未启用括号粘贴保护。")
            color: themeManager.textPrimary; wrapMode: Text.WordWrap; font.pixelSize: 11
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 8
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text { text: "受限终端"; color: themeManager.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text { Layout.fillWidth: true; text: terminalManager.statusText; color: terminalManager.running || terminalManager.sessionStarting ? themeManager.warning : themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
            }
            ActionButton { visible: terminalManager.sessionActive || terminalManager.sessionStarting || terminalManager.running; label: "结束会话"; danger: true; onClicked: terminalManager.stop() }
            ActionButton { label: "日志"; onClicked: terminalManager.openLogFile() }
            ActionButton { label: "清屏"; onClicked: terminalManager.clear() }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: controlsColumn.implicitHeight + 18; radius: 12
            color: themeManager.surface; border.width: 1; border.color: themeManager.border
            ColumnLayout {
                id: controlsColumn
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 9; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    ComboBox {
                        id: transportChooser; objectName: "terminalTransportChooser"
                        Layout.preferredWidth: Math.min(158, root.width * 0.36)
                        model: ["自动 · 原生优先", "原生 ConPTY", "兼容管道"]
                        currentIndex: terminalManager.preferredTransport === "pipe" ? 2 : terminalManager.preferredTransport === "conpty" ? 1 : 0
                        enabled: !terminalManager.sessionActive && !terminalManager.sessionStarting && !terminalManager.running
                        onActivated: index => { terminalManager.preferredTransport = ["auto", "conpty", "pipe"][index]; root.notice = "传输模式已选择；不会自动启动 Shell" }
                        font.pixelSize: 10
                    }
                    Text { Layout.fillWidth: true; text: terminalManager.shellName + " · " + terminalManager.transportName; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
                    Text { visible: terminalManager.running || terminalManager.sessionActive; text: root.formatElapsed(terminalManager.elapsedMilliseconds); color: themeManager.accent; font.family: "Cascadia Mono"; font.pixelSize: 10 }
                    ActionButton { visible: !terminalManager.sessionActive && !terminalManager.sessionStarting; label: "启动会话"; onClicked: { terminalManager.startSession(); root.focusTerminal() } }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { Layout.fillWidth: true; text: (root.nativeMode ? "启动目录 · " : "当前目录 · ") + terminalManager.currentDirectory; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideMiddle }
                    Text { text: "UTF-8"; color: themeManager.textFaint; font.pixelSize: 9 }
                    ComboBox {
                        visible: !root.nativeMode; objectName: "terminalTimeoutChooser"
                        Layout.preferredWidth: 105; model: ["超时 30 秒", "超时 60 秒", "超时 5 分钟", "关闭超时"]
                        currentIndex: terminalManager.commandTimeoutSeconds === 0 ? 3 : terminalManager.commandTimeoutSeconds === 30 ? 0 : terminalManager.commandTimeoutSeconds === 300 ? 2 : 1
                        onActivated: index => terminalManager.commandTimeoutSeconds = [30, 60, 300, 0][index]
                        font.pixelSize: 9
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Text { Layout.fillWidth: true; text: root.nativeMode ? (root.screen.title || "原生屏幕 · 按键直接传递") : "命令输入 · 兼容模式"; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
            ActionButton { visible: root.nativeMode; label: root.assistantVisible ? "关闭辅助" : "辅助输入"; onClicked: { root.assistantVisible = !root.assistantVisible; root.focusTerminal() } }
            ActionButton { label: root.copyMode ? "返回输入" : "选择输出"; onClicked: { root.copyMode = !root.copyMode; if (!root.copyMode) root.focusTerminal() } }
            ActionButton { label: "复制全部"; onClicked: root.copyAllOutput() }
        }
        Rectangle {
            id: terminalPanel
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 150
            radius: 12; color: "#0E1117"; border.width: 1; border.color: rawTextInput.activeFocus || commandInput.activeFocus ? themeManager.accent : themeManager.border; clip: true
            Flickable {
                id: nativeScroll; objectName: "nativeTerminalScreen"
                visible: root.nativeMode && !root.copyMode
                anchors.fill: parent; anchors.margins: 10
                contentWidth: Math.max(width, root.screen.columns * root.cellWidth)
                contentHeight: Math.max(height, (root.screen.scrollbackRows + root.screen.rows) * root.cellHeight)
                boundsBehavior: Flickable.StopAtBounds
                onWidthChanged: root.scheduleResize()
                onHeightChanged: root.scheduleResize()
                onMovementStarted: root.followOutput = false
                onMovementEnded: root.followOutput = contentY >= Math.max(0, contentHeight - height) - root.cellHeight
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                TerminalCanvas {
                    objectName: "terminalCellCanvas"
                    y: nativeScroll.contentY
                    width: nativeScroll.width; height: nativeScroll.height
                    screen: root.screen; font: terminalMetrics.font
                    cellWidth: root.cellWidth; cellHeight: root.cellHeight
                    scrollOffset: nativeScroll.contentY
                }
                Rectangle {
                    x: root.screen.cursorColumn * root.cellWidth; y: root.screen.cursorRow * root.cellHeight
                    width: root.cellWidth; height: root.cellHeight
                    visible: root.screen.cursorVisible && rawTextInput.activeFocus && !rawTextInput.inputMethodComposing && terminalManager.sessionActive
                    color: "#80D7E0EA"
                }
                TextInput {
                    id: rawTextInput; objectName: "nativeTerminalInput"
                    x: root.screen.cursorColumn * root.cellWidth; y: root.screen.cursorRow * root.cellHeight
                    width: Math.max(root.cellWidth, nativeScroll.width - x); height: root.cellHeight
                    color: "#D7E0EA"; font: terminalMetrics.font; opacity: inputMethodComposing ? 1 : 0
                    readOnly: !terminalManager.sessionActive || !terminalManager.transportInteractive
                    activeFocusOnTab: false; selectByMouse: false; cursorDelegate: Rectangle { width: 1; color: "transparent" }
                    inputMethodHints: Qt.ImhNoPredictiveText
                    KeyNavigation.tab: rawTextInput; KeyNavigation.backtab: rawTextInput
                    Keys.priority: Keys.BeforeItem; Keys.onPressed: event => root.nativeKey(event)
                    onTextEdited: root.flushCommittedInput()
                    onInputMethodComposingChanged: if (!inputMethodComposing) Qt.callLater(root.flushCommittedInput)
                }
                TapHandler { onTapped: { if (!rawTextInput.inputMethodComposing) rawTextInput.forceActiveFocus() } }
            }
            ScrollView {
                visible: !root.nativeMode || root.copyMode; anchors.fill: parent; anchors.margins: 8
                TextArea {
                    id: terminalOutput; objectName: "terminalOutputSelection"
                    text: root.nativeMode ? (root.copyMode ? root.screen.plainOutput : "") : terminalManager.output
                    readOnly: true; textFormat: TextEdit.PlainText; selectByMouse: true
                    wrapMode: root.nativeMode ? TextEdit.NoWrap : TextEdit.WrapAnywhere
                    color: "#D7E0EA"; selectionColor: themeManager.accent; font: terminalMetrics.font; background: null
                    onTextChanged: if (!root.copyMode) cursorPosition = length
                    Keys.onPressed: event => { if (event.key === Qt.Key_Escape) { root.copyMode = false; root.focusTerminal(); event.accepted = true } }
                }
            }
            Column {
                visible: !terminalManager.sessionActive && terminalManager.output.length === 0 && !root.copyMode
                anchors.centerIn: parent; width: Math.max(0, parent.width - 32); spacing: 8
                Text { width: parent.width; text: root.nativeMode ? "准备开始原生终端" : "准备开始兼容命令会话"; color: "#D7E0EA"; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
                Text { width: parent.width; text: "Shell 不会自动启动。\nConPTY 支持颜色、光标、交互程序；兼容模式保留命令结果与超时。"; color: "#8492A6"; font.pixelSize: 10; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
            }
            ActionButton { visible: root.nativeMode && !root.followOutput; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; label: "最新输出"; onClicked: { root.followOutput = true; root.scrollToBottom() } }
        }
        Rectangle {
            visible: !root.nativeMode || root.assistantVisible
            Layout.fillWidth: true; implicitHeight: editorColumn.implicitHeight + 18; radius: 12
            color: themeManager.surface; border.width: 1; border.color: themeManager.border
            ColumnLayout {
                id: editorColumn
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 9; spacing: 5
                Text { Layout.fillWidth: true; text: root.nativeMode ? "辅助输入 · Enter 只插入，不执行 · 不替换 Shell 的 Tab 补全" : "命令输入 · Enter 执行 · Tab 接受预览 · Shift+Tab 切换"; color: themeManager.textFaint; font.pixelSize: 9; wrapMode: Text.WordWrap }
                RowLayout {
                    Layout.fillWidth: true; spacing: 7
                    Text { text: root.nativeMode ? ">_" : (terminalManager.running ? "●" : "命令>"); color: terminalManager.running ? themeManager.warning : themeManager.success; font.family: "Cascadia Mono"; font.pixelSize: 11 }
                    TextInput {
                        id: commandInput; objectName: "terminalCommandInput"
                        Layout.fillWidth: true; Layout.preferredHeight: 26
                        readOnly: !root.nativeMode && terminalManager.running
                        color: themeManager.textPrimary; selectionColor: themeManager.accent
                        font.family: "Cascadia Mono"; font.pixelSize: 11; selectByMouse: true; activeFocusOnTab: false
                        KeyNavigation.tab: commandInput; KeyNavigation.backtab: commandInput
                        Keys.priority: Keys.BeforeItem
                        onTextChanged: { root.paletteIndex = 0; completionDebounce.restart() }
                        Keys.onPressed: event => {
                            if (inputMethodComposing) return
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { root.submitCommand(); event.accepted = true }
                            else if (event.key === Qt.Key_Tab) { root.completeCommand(); event.accepted = true }
                            else if (event.key === Qt.Key_Backtab) { root.moveCandidate(-1); event.accepted = true }
                            else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                                if (root.candidates.length) root.moveCandidate(event.key === Qt.Key_Up ? -1 : 1)
                                else root.historyStep(event.key === Qt.Key_Up ? -1 : 1)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                if (!root.nativeMode && terminalManager.running) terminalManager.stop()
                                else if (root.nativeMode) { root.assistantVisible = false; rawTextInput.forceActiveFocus() }
                                else { clear(); root.notice = ""; root.reverseSearchIndex = -1 }
                                event.accepted = true
                            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C && terminalManager.running && !root.nativeMode) { terminalManager.stop(); event.accepted = true }
                            else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_L) { terminalManager.clear(); event.accepted = true }
                            else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_R) { root.reverseSearchHistory(); event.accepted = true }
                            else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Space && root.nativeMode) { root.assistantVisible = false; rawTextInput.forceActiveFocus(); event.accepted = true }
                        }
                    }
                    ActionButton { label: root.nativeMode ? "插入草稿" : "执行"; enabled: commandInput.text.trim().length > 0 && (root.nativeMode ? terminalManager.sessionActive : !terminalManager.running); onClicked: root.submitCommand() }
                }
                Text {
                    objectName: "terminalCompletionPreview"; visible: root.candidates.length > 0; Layout.fillWidth: true
                    text: root.candidates.length ? "预览  " + root.completedDraft(root.candidates[Math.min(root.paletteIndex, root.candidates.length - 1)]) : ""
                    color: themeManager.accent; font.family: "Cascadia Mono"; font.pixelSize: 10; elide: Text.ElideRight
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Repeater {
                        model: root.candidates
                        Rectangle {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true; implicitHeight: 26; radius: 6
                            color: index === root.paletteIndex ? themeManager.accentSoft : "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 6
                                Text { Layout.maximumWidth: root.width * 0.45; text: modelData.name; color: themeManager.accent; font.family: "Cascadia Mono"; font.pixelSize: 10; elide: Text.ElideMiddle }
                                Text { text: modelData.kind; color: themeManager.textFaint; font.pixelSize: 8 }
                                Text { Layout.fillWidth: true; text: modelData.description; color: themeManager.textMuted; font.pixelSize: 9; elide: Text.ElideRight }
                            }
                            TapHandler { onTapped: root.acceptCompletion(modelData) }
                        }
                    }
                }
            }
        }
        Text { Layout.fillWidth: true; visible: root.notice.length > 0; text: root.notice; color: themeManager.textMuted; font.pixelSize: 9; wrapMode: Text.WordWrap }
        Text { Layout.fillWidth: true; text: "实验性限制，不是安全沙盒。程序仍以当前用户权限运行；结束会话会回收所属进程。原生 Ctrl+C 为中断，工具栏“结束会话”为强制停止。"; color: themeManager.warning; font.pixelSize: 8; wrapMode: Text.WordWrap }
    }
    component ActionButton: Rectangle {
        id: action
        property string label
        property bool danger: false
        signal clicked()
        implicitWidth: Math.max(46, actionText.implicitWidth + 16); implicitHeight: 30; radius: 9
        opacity: enabled ? 1 : 0.45
        color: actionHover.hovered && enabled ? (danger ? themeManager.dangerSoft : themeManager.accentSoft) : themeManager.surface
        border.width: 1; border.color: danger ? themeManager.danger : themeManager.border
        Text { id: actionText; anchors.centerIn: parent; text: action.label; color: action.danger ? themeManager.danger : themeManager.accent; font.pixelSize: 9; font.weight: Font.DemiBold }
        HoverHandler { id: actionHover }
        TapHandler { enabled: action.enabled; onTapped: action.clicked() }
    }
}
