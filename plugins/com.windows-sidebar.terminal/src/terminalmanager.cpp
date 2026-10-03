#include "include/terminalmanager.h"
#include "include/terminalscreen.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QSet>
#include <QTimer>
#include <QUuid>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {
QByteArray removeJsonCommentsAndTrailingCommas(const QByteArray &input)
{
    QByteArray output;
    bool inString = false, escaped = false, lineComment = false, blockComment = false;
    for (qsizetype i = 0; i < input.size(); ++i) {
        const char current = input.at(i);
        const char next = i + 1 < input.size() ? input.at(i + 1) : '\0';
        if (lineComment) { if (current == '\n') { lineComment = false; output += current; } continue; }
        if (blockComment) { if (current == '*' && next == '/') { blockComment = false; ++i; } continue; }
        if (!inString && current == '/' && next == '/') { lineComment = true; ++i; continue; }
        if (!inString && current == '/' && next == '*') { blockComment = true; ++i; continue; }
        output += current;
        if (inString) {
            if (escaped) escaped = false;
            else if (current == '\\') escaped = true;
            else if (current == '"') inString = false;
        } else if (current == '"') inString = true;
    }
    QString cleaned = QString::fromUtf8(output);
    cleaned.replace(QRegularExpression(QStringLiteral(",\\s*([}\\]])")), QStringLiteral("\\1"));
    return cleaned.toUtf8();
}

QPair<QString, QString> detectDefaultShell()
{
    const QString local = QProcessEnvironment::systemEnvironment().value(QStringLiteral("LOCALAPPDATA"));
    const QStringList candidates = {
        local + QStringLiteral("/Packages/Microsoft.WindowsTerminal_8wekyb3d8bbwe/LocalState/settings.json"),
        local + QStringLiteral("/Packages/Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe/LocalState/settings.json"),
        local + QStringLiteral("/Microsoft/Windows Terminal/settings.json")
    };
    for (const QString &path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QJsonObject root = QJsonDocument::fromJson(removeJsonCommentsAndTrailingCommas(file.readAll())).object();
        const QString defaultProfile = root.value(QStringLiteral("defaultProfile")).toString();
        const QJsonArray profiles = root.value(QStringLiteral("profiles")).toObject().value(QStringLiteral("list")).toArray();
        for (const QJsonValue &value : profiles) {
            const QJsonObject profile = value.toObject();
            if (profile.value(QStringLiteral("guid")).toString().compare(defaultProfile, Qt::CaseInsensitive) != 0
                && profile.value(QStringLiteral("name")).toString().compare(defaultProfile, Qt::CaseInsensitive) != 0)
                continue;
            const QString name = profile.value(QStringLiteral("name")).toString(QStringLiteral("系统默认"));
            const QString commandLine = profile.value(QStringLiteral("commandline")).toString();
            if (!commandLine.isEmpty()) {
                QString program = QProcess::splitCommand(commandLine).value(0);
#ifdef Q_OS_WIN
                wchar_t expanded[32768]{};
                const DWORD count = ExpandEnvironmentStringsW(reinterpret_cast<LPCWSTR>(program.utf16()), expanded, 32768);
                if (count > 0 && count < 32768) program = QString::fromWCharArray(expanded);
#endif
                const QString base = QFileInfo(program).fileName().toLower();
                if (base == QStringLiteral("pwsh.exe") || base == QStringLiteral("powershell.exe") || base == QStringLiteral("cmd.exe"))
                    return {program, name};
            }
            const QString source = profile.value(QStringLiteral("source")).toString();
            if (source.contains(QStringLiteral("PowershellCore"), Qt::CaseInsensitive))
                return {QStringLiteral("pwsh.exe"), name};
            if (name.contains(QStringLiteral("command"), Qt::CaseInsensitive) || name.contains(QStringLiteral("cmd"), Qt::CaseInsensitive))
                return {QStringLiteral("cmd.exe"), name};
            if (name.contains(QStringLiteral("PowerShell"), Qt::CaseInsensitive))
                return {QStringLiteral("powershell.exe"), name};
        }
    }
    return {QStringLiteral("powershell.exe"), QStringLiteral("Windows PowerShell（回退）")};
}
}

TerminalManager::TerminalManager(QObject *parent, const QString &dataRoot) : QObject(parent)
{
    m_screen = new TerminalScreen(this);
    connect(m_screen, &TerminalScreen::responseRequested, this, [this](const QString &reply) {
        if (sessionActive() && m_transportInteractive) sendInput(reply);
    });
    m_commandTimeout.setSingleShot(true);
    connect(&m_commandTimeout, &QTimer::timeout, this, [this] {
        if (!m_commandRunning) return;
        writeLog(QStringLiteral("command.timeout"));
        requestClose(QStringLiteral("timeout"));
    });
    m_startTimeout.setSingleShot(true);
    m_startTimeout.setInterval(7000);
    connect(&m_startTimeout, &QTimer::timeout, this, [this] {
        if (m_transportDecided) return;
        m_statusText = QStringLiteral("Runner 启动或传输握手超时");
        emit statusChanged();
        requestClose(QStringLiteral("start-failed"));
    });
    m_elapsedPulse.setInterval(250);
    connect(&m_elapsedPulse, &QTimer::timeout, this, &TerminalManager::elapsedChanged);
#ifdef Q_OS_WIN
    const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    m_conPtyAvailable = kernel
        && GetProcAddress(kernel, "CreatePseudoConsole")
        && GetProcAddress(kernel, "ResizePseudoConsole")
        && GetProcAddress(kernel, "ClosePseudoConsole");
#endif
    m_transportName = QStringLiteral("自动选择 · 尚未启动");
    const QString storage = dataRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : dataRoot;
    m_sandboxPath = storage
        + QStringLiteral("/terminal-sandbox");
    QDir().mkpath(m_sandboxPath);
    m_sandboxPath = QDir(m_sandboxPath).absolutePath();
    m_currentDirectory = m_sandboxPath;
    const QString logDirectory = storage
        + QStringLiteral("/logs");
    QDir().mkpath(logDirectory);
    m_logPath = logDirectory + QStringLiteral("/terminal-plugin.log");
    const auto shell = detectDefaultShell();
    m_shellProgram = shell.first;
    m_shellName = shell.second + QStringLiteral(" · ") + QFileInfo(shell.first).fileName();
    QSet<QString> commands = {
        QStringLiteral("cd"), QStringLiteral("clear"), QStringLiteral("cls"), QStringLiteral("copy"),
        QStringLiteral("date"), QStringLiteral("dir"), QStringLiteral("echo"), QStringLiteral("exit"),
        QStringLiteral("Get-ChildItem"), QStringLiteral("Get-Command"), QStringLiteral("Get-Content"),
        QStringLiteral("Get-Date"), QStringLiteral("Get-Help"), QStringLiteral("Get-Location"),
        QStringLiteral("Get-Process"), QStringLiteral("git"), QStringLiteral("mkdir"),
        QStringLiteral("Move-Item"), QStringLiteral("pwd"), QStringLiteral("Remove-Item"),
        QStringLiteral("Set-Location"), QStringLiteral("where"), QStringLiteral("whoami")
    };
    const QStringList pathDirectories = QProcessEnvironment::systemEnvironment()
        .value(QStringLiteral("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    const QStringList executableFilters = {QStringLiteral("*.exe"), QStringLiteral("*.cmd"),
                                            QStringLiteral("*.bat"), QStringLiteral("*.com")};
    for (const QString &directory : pathDirectories) {
        const QFileInfoList entries = QDir(directory).entryInfoList(executableFilters, QDir::Files);
        for (const QFileInfo &entry : entries)
            commands.insert(entry.completeBaseName());
    }
    m_availableCommands = commands.values();
    m_availableCommands.sort(Qt::CaseInsensitive);
    writeLog(QStringLiteral("manager.init"), QStringLiteral("shell=%1 program=%2 sandbox=%3 conPtyAvailable=%4")
        .arg(m_shellName, m_shellProgram, m_sandboxPath).arg(m_conPtyAvailable));
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] { appendOutput(m_process.readAllStandardOutput()); });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        // Diagnostics are not trusted as protocol frames or command completion markers.
        appendVisible(m_process.readAllStandardError());
    });
    connect(&m_process, &QProcess::stateChanged, this, [this] { emit sessionChanged(); });
    connect(&m_process, &QProcess::started, this, [this] {
        writeLog(QStringLiteral("session.started"), QStringLiteral("shell=%1 cwd=%2 requestedTransport=%3")
            .arg(m_shellProgram, m_currentDirectory, m_preferredTransport));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        appendOutput(m_process.readAllStandardOutput());
        appendVisible(m_process.readAllStandardError());
        if (!m_protocolPending.isEmpty()) { appendVisible(m_protocolPending); m_protocolPending.clear(); }
        m_commandTimeout.stop();
        m_startTimeout.stop();
        const QString outcome = !m_terminationOutcome.isEmpty() ? m_terminationOutcome
            : !m_transportDecided ? QStringLiteral("start-failed")
            : status == QProcess::CrashExit ? QStringLiteral("crashed")
            : code == 0 ? QStringLiteral("success") : QStringLiteral("failed");
        if (m_commandRunning) finishCommand(outcome, code);
        else finishSession(outcome, code);
        m_pendingCommand.clear();
        m_pendingInput.clear();
        m_commandMarker.clear();
        m_statusText = outcome == QStringLiteral("reset") ? QStringLiteral("会话已重置，下次打开时启动 Shell")
            : outcome == QStringLiteral("cancelled") ? QStringLiteral("Shell 会话已停止")
            : outcome == QStringLiteral("timeout") ? QStringLiteral("命令超时，Runner 作业树已回收")
            : status == QProcess::NormalExit ? QStringLiteral("Shell 会话已关闭") : QStringLiteral("Shell 会话异常终止");
        writeLog(QStringLiteral("session.finished"), QStringLiteral("exitCode=%1 exitStatus=%2").arg(code).arg(static_cast<int>(status)));
        m_terminationOutcome.clear();
        if (m_resetPending) { m_resetPending = false; resetLocalState(); }
        m_elapsedPulse.stop();
        emit elapsedChanged();
        emit sessionChanged();
        emit statusChanged();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        m_statusText = QStringLiteral("Runner 错误：") + m_process.errorString();
        appendVisible(QStringLiteral("\n[Runner 错误] %1\n").arg(m_process.errorString()).toUtf8());
        writeLog(QStringLiteral("runner.error"), QStringLiteral("type=%1 message=%2")
            .arg(static_cast<int>(error)).arg(m_process.errorString()));
        if (error == QProcess::FailedToStart) {
            m_startTimeout.stop();
            if (m_commandRunning) finishCommand(QStringLiteral("start-failed"), -1);
            else finishSession(QStringLiteral("start-failed"), -1);
            m_pendingCommand.clear(); m_pendingInput.clear();
            m_elapsedPulse.stop();
            m_terminationOutcome.clear();
            if (m_resetPending) { m_resetPending = false; resetLocalState(); }
        }
        emit statusChanged(); emit sessionChanged();
    });
}

bool TerminalManager::ensureSession()
{
    if (!m_terminationOutcome.isEmpty() || m_resetPending) return false;
    if (m_process.state() != QProcess::NotRunning) return true;
    const QString runner = QCoreApplication::applicationDirPath() + QStringLiteral("/sidebar-terminal-runner.exe");
    QStringList arguments = {QStringLiteral("--session"), QStringLiteral("--cwd"), m_currentDirectory,
                             QStringLiteral("--shell"), m_shellProgram, QStringLiteral("--log"), m_logPath,
                             QStringLiteral("--transport"), m_preferredTransport,
                             QStringLiteral("--columns"), QString::number(m_columns),
                             QStringLiteral("--rows"), QString::number(m_rows)};
    for (auto iterator = m_sessionEnvironment.constBegin(); iterator != m_sessionEnvironment.constEnd(); ++iterator) {
        arguments += QStringLiteral("--env64");
        arguments += iterator.key() + QLatin1Char(':') + QString::fromLatin1(iterator.value().toUtf8().toBase64());
    }
    m_transportDecided = false;
    ++m_sessionGeneration;
    m_screen->resetForSession();
    m_protocolPending.clear();
    m_sessionOutcomeEmitted = false;
    m_sessionTimer.start();
    m_utf8Decoder = QStringDecoder(QStringConverter::Utf8);
    m_escapeState = EscapeState::Normal;
    m_statusText = QStringLiteral("正在启动 Shell…");
    m_process.start(runner, arguments);
    m_startTimeout.start();
    emit statusChanged();
    return true;
}

TerminalManager::~TerminalManager()
{
    m_commandTimeout.stop();
    m_startTimeout.stop();
    m_elapsedPulse.stop();
    m_destroying = true;
    m_process.disconnect(this);
    m_process.kill();
    // The worker's Job Object owns its descendants; bounded shutdown only.
    m_process.waitForFinished(1500);
}

void TerminalManager::resetSession()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_resetPending = true;
        requestClose(QStringLiteral("reset"));
    }
    resetLocalState();
}

void TerminalManager::resetLocalState()
{
    m_sessionEnvironment.clear();
    m_currentDirectory = m_sandboxPath;
    m_utf8Decoder = QStringDecoder(QStringConverter::Utf8);
    m_escapeState = EscapeState::Normal;
    clear();
    m_screen->resetForSession();
    m_pendingCommand.clear(); m_pendingInput.clear();
    m_statusText = m_process.state() == QProcess::NotRunning
        ? QStringLiteral("会话已重置，下次打开时启动 Shell") : QStringLiteral("正在重置 Shell 会话…");
    emit currentDirectoryChanged();
    emit statusChanged();
}

QObject *TerminalManager::screen() const { return m_screen; }

qint64 TerminalManager::elapsedMilliseconds() const
{
    if (m_commandRunning && m_runTimer.isValid()) return m_runTimer.elapsed();
    if (m_transportInteractive && sessionActive() && m_sessionTimer.isValid()) return m_sessionTimer.elapsed();
    return m_lastElapsed;
}

void TerminalManager::setPreferredTransport(const QString &transport)
{
    const QString mode = transport.toLower();
    if (mode != QStringLiteral("auto") && mode != QStringLiteral("pipe") && mode != QStringLiteral("conpty")) return;
    if (m_preferredTransport == mode) return;
    resetSession();
    m_preferredTransport = mode;
    m_transportInteractive = false;
    m_transportDecided = false;
    m_transportName = mode == QStringLiteral("pipe") ? QStringLiteral("兼容管道 · 尚未启动") : QStringLiteral("原生终端 · 尚未启动");
    emit transportChanged();
}

void TerminalManager::setCommandTimeoutSeconds(int seconds)
{
    const int bounded = qBound(0, seconds, 24 * 60 * 60);
    if (bounded == m_commandTimeoutSeconds) return;
    m_commandTimeoutSeconds = bounded;
    if (m_commandRunning) {
        m_commandTimeout.stop();
        if (bounded > 0) {
            const qint64 remaining = qint64(bounded) * 1000 - m_runTimer.elapsed();
            m_commandTimeout.start(int(qMax<qint64>(1, remaining)));
        }
    }
    emit statusChanged();
}

void TerminalManager::startSession() { ensureSession(); }

void TerminalManager::resizeTerminal(int columns, int rows)
{
    m_columns = qBound(20, columns, 240);
    m_rows = qBound(5, rows, 100);
    m_screen->resize(m_columns, m_rows);
    if (sessionActive() && m_transportDecided && m_transportInteractive)
        m_process.write("S\t" + QByteArray::number(m_columns) + '\t' + QByteArray::number(m_rows) + '\n');
}

void TerminalManager::sendInput(const QString &input)
{
    if (input.isEmpty() || !m_terminationOutcome.isEmpty()) return;
    const QByteArray bytes = input.toUtf8();
    if (bytes.size() > 65536 || m_process.bytesToWrite() + bytes.size() > 256 * 1024) {
        m_statusText = QStringLiteral("输入过大或 Shell 输入积压；本次输入未执行");
        emit statusChanged(); return;
    }
    if (m_transportDecided && !m_transportInteractive) return;
    if (m_pendingInput.size() + bytes.size() > 65536) {
        m_statusText = QStringLiteral("启动期间输入过多，请等待 Shell 就绪");
        emit statusChanged(); return;
    }
    m_pendingInput += bytes;
    if (ensureSession()) dispatchPendingInput();
}

void TerminalManager::dispatchPendingInput()
{
    if (!sessionActive() || !m_transportDecided || !m_terminationOutcome.isEmpty()) return;
    if (!m_pendingCommand.isEmpty()) {
        const QString command = m_pendingCommand;
        m_pendingCommand.clear();
        if (m_transportInteractive) {
            m_process.write("I\t" + (command + QLatin1Char('\r')).toUtf8().toBase64() + '\n');
        } else if (m_commandRunning) {
            m_process.write("R\t" + m_commandMarker.mid(QByteArray("__SIDEBAR_DONE_").size())
                + '\t' + command.toUtf8().toBase64() + '\n');
        } else {
            run(command);
        }
    }
    if (m_transportInteractive && !m_pendingInput.isEmpty())
        m_process.write("I\t" + m_pendingInput.toBase64() + '\n');
    else if (!m_transportInteractive && !m_pendingInput.isEmpty()) {
        m_statusText = QStringLiteral("已回退兼容管道；启动前的原生按键未执行，请使用命令输入区");
        emit statusChanged();
    }
    m_pendingInput.clear();
}

void TerminalManager::finishCommand(const QString &outcome, int exitCode, bool naturalCompletion)
{
    if (!m_commandRunning) return;
    m_lastElapsed = m_runTimer.isValid() ? m_runTimer.elapsed() : 0;
    m_commandRunning = false;
    m_commandTimeout.stop();
    m_elapsedPulse.stop();
    m_commandMarker.clear();
    m_statusText = outcome == QStringLiteral("success") ? QStringLiteral("命令已完成 · Shell 会话保持")
        : QStringLiteral("命令结束 · %1 · 退出码 %2").arg(outcome).arg(exitCode);
    appendVisible(QStringLiteral("\n[%1] 退出码 %2 · %3 ms\n").arg(outcome).arg(exitCode).arg(m_lastElapsed).toUtf8());
    writeLog(QStringLiteral("command.finished"), QStringLiteral("outcome=%1 exitCode=%2 elapsedMs=%3 outputBytes=%4")
        .arg(outcome).arg(exitCode).arg(m_lastElapsed).arg(m_outputBytes));
    emit runningChanged(); emit statusChanged(); emit elapsedChanged();
    emit operationFinished(QStringLiteral("command"), outcome, exitCode, m_lastElapsed);
    if (naturalCompletion) emit commandCompleted(exitCode, m_lastElapsed);
}

void TerminalManager::finishSession(const QString &outcome, int exitCode)
{
    if (m_sessionOutcomeEmitted) return;
    m_sessionOutcomeEmitted = true;
    m_lastElapsed = m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0;
    if (!m_destroying) emit operationFinished(QStringLiteral("session"), outcome, exitCode, m_lastElapsed);
}

void TerminalManager::requestClose(const QString &outcome)
{
    if (m_process.state() == QProcess::NotRunning || !m_terminationOutcome.isEmpty()) return;
    m_terminationOutcome = outcome;
    m_statusText = outcome == QStringLiteral("timeout") ? QStringLiteral("命令超时，正在回收作业树…") : QStringLiteral("正在关闭 Shell 作业树…");
    m_commandTimeout.stop(); m_startTimeout.stop();
    if (m_process.state() == QProcess::Running) m_process.write("X\n");
    const quint64 generation = m_sessionGeneration;
    QTimer::singleShot(800, this, [this, generation] {
        if (generation == m_sessionGeneration && !m_terminationOutcome.isEmpty()
            && m_process.state() != QProcess::NotRunning) m_process.kill();
    });
    emit statusChanged();
}

QStringList TerminalManager::completionSuggestions(const QString &input) const
{
    const QString text = input;
    if (text.trimmed().isEmpty() || text.trimmed().startsWith(QLatin1Char('/')))
        return {};

    const int tokenStart = completionStart(text);
    QString token = text.mid(tokenStart);
    const QChar quote = !token.isEmpty() && (token.front() == QLatin1Char('"') || token.front() == QLatin1Char('\''))
        ? token.front() : QChar();
    if (!quote.isNull()) {
        token.remove(0, 1);
        if (token.endsWith(quote)) token.chop(1);
    }
    if (token.isEmpty()) return {};

    QStringList results;
    constexpr qsizetype maximumResults = 8;
    if (tokenStart == 0) {
        for (const QString &command : m_availableCommands) {
            if (!command.startsWith(token, Qt::CaseInsensitive) || command.compare(token, Qt::CaseInsensitive) == 0)
                continue;
            results += command + QStringLiteral("\t命令\t来自 Shell 与系统 PATH");
            if (results.size() >= maximumResults) break;
        }
        return results;
    }

    // A bounded, declarative catalog avoids evaluating user input or invoking a
    // possibly shadowed Get-Command function just to render completion previews.
    static const QHash<QString, QStringList> parameters = {
        {QStringLiteral("get-childitem"), {QStringLiteral("-Path"), QStringLiteral("-LiteralPath"), QStringLiteral("-Filter"), QStringLiteral("-Recurse"), QStringLiteral("-Force"), QStringLiteral("-Directory"), QStringLiteral("-File")}},
        {QStringLiteral("get-content"), {QStringLiteral("-Path"), QStringLiteral("-LiteralPath"), QStringLiteral("-Raw"), QStringLiteral("-Tail"), QStringLiteral("-TotalCount"), QStringLiteral("-Encoding")}},
        {QStringLiteral("get-command"), {QStringLiteral("-Name"), QStringLiteral("-CommandType"), QStringLiteral("-Module"), QStringLiteral("-All")}},
        {QStringLiteral("get-help"), {QStringLiteral("-Name"), QStringLiteral("-Examples"), QStringLiteral("-Detailed"), QStringLiteral("-Full"), QStringLiteral("-Online")}},
        {QStringLiteral("set-location"), {QStringLiteral("-Path"), QStringLiteral("-LiteralPath"), QStringLiteral("-PassThru")}},
        {QStringLiteral("get-process"), {QStringLiteral("-Name"), QStringLiteral("-Id"), QStringLiteral("-FileVersionInfo")}},
        {QStringLiteral("remove-item"), {QStringLiteral("-Path"), QStringLiteral("-LiteralPath"), QStringLiteral("-Recurse"), QStringLiteral("-Force"), QStringLiteral("-WhatIf"), QStringLiteral("-Confirm")}},
        {QStringLiteral("copy-item"), {QStringLiteral("-Path"), QStringLiteral("-LiteralPath"), QStringLiteral("-Destination"), QStringLiteral("-Recurse"), QStringLiteral("-Force"), QStringLiteral("-WhatIf")}}
    };
    QString command = text.left(tokenStart).trimmed().section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).toLower();
    if (command == QStringLiteral("ls") || command == QStringLiteral("dir")) command = QStringLiteral("get-childitem");
    if (command == QStringLiteral("cd")) command = QStringLiteral("set-location");
    if (token.startsWith(QLatin1Char('-')) && !m_shellProgram.endsWith(QStringLiteral("cmd.exe"), Qt::CaseInsensitive)) {
        for (const QString &parameter : parameters.value(command)) {
            if (parameter.startsWith(token, Qt::CaseInsensitive) && parameter.compare(token, Qt::CaseInsensitive) != 0)
                results += parameter + QStringLiteral("\t参数\t常用 PowerShell 参数目录");
            if (results.size() >= maximumResults) break;
        }
        return results;
    }
    QString normalizedToken = token;
    normalizedToken.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int separator = normalizedToken.lastIndexOf(QLatin1Char('/'));
    const QString relativeDirectory = separator >= 0 ? normalizedToken.left(separator + 1) : QString();
    const QString namePrefix = separator >= 0 ? normalizedToken.mid(separator + 1) : normalizedToken;
    QDir directory(QDir(m_currentDirectory).filePath(relativeDirectory));
    const QString canonical = directory.canonicalPath();
    const QString rootPath = QDir(m_sandboxPath).canonicalPath();
    if (canonical.isEmpty() || (canonical.compare(rootPath, Qt::CaseInsensitive) != 0
        && !canonical.startsWith(rootPath + QLatin1Char('/'), Qt::CaseInsensitive))) return {};
    const QFileInfoList entries = directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                           QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        if (!entry.fileName().startsWith(namePrefix, Qt::CaseInsensitive)) continue;
        QString value = relativeDirectory + entry.fileName();
        if (entry.isDir()) value += QLatin1Char('/');
        if (!quote.isNull()) {
            if (!m_shellProgram.endsWith(QStringLiteral("cmd.exe"), Qt::CaseInsensitive)) {
                if (quote == QLatin1Char('\'')) value.replace(QLatin1Char('\''), QStringLiteral("''"));
                else { value.replace(QLatin1Char('`'), QStringLiteral("``")); value.replace(QLatin1Char('$'), QStringLiteral("`$")); }
            }
            value = quote + value + quote;
        }
        else if (value.contains(QRegularExpression(QStringLiteral("[\\s;|&()'`$]")))) {
            const bool commandPrompt = m_shellProgram.endsWith(QStringLiteral("cmd.exe"), Qt::CaseInsensitive);
            value = commandPrompt ? QLatin1Char('"') + value + QLatin1Char('"')
                : QLatin1Char('\'') + value.replace(QLatin1Char('\''), QStringLiteral("''")) + QLatin1Char('\'');
        }
        results += value + (entry.isDir()
            ? QStringLiteral("\t目录\t沙盒中的目录")
            : QStringLiteral("\t文件\t沙盒中的文件"));
        if (results.size() >= maximumResults) break;
    }
    return results;
}

int TerminalManager::completionStart(const QString &input) const
{
    QChar quote;
    bool escaped = false;
    int start = 0;
    for (int index = 0; index < input.size(); ++index) {
        const QChar character = input.at(index);
        if (escaped) { escaped = false; continue; }
        if (character == QLatin1Char('`')) { escaped = true; continue; }
        if (!quote.isNull()) {
            if (character == quote) {
                if (index + 1 < input.size() && input.at(index + 1) == quote) { ++index; continue; }
                quote = QChar();
            }
        } else if (character == QLatin1Char('"') || character == QLatin1Char('\'')) quote = character;
        else if (character.isSpace() || character == QLatin1Char('|') || character == QLatin1Char(';')) start = index + 1;
    }
    return start;
}

void TerminalManager::run(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty() || running()) return;
    if (trimmed.toUtf8().size() > 65536) {
        m_statusText = QStringLiteral("命令超过 64 KiB，未执行"); emit statusChanged(); return;
    }
    if (!m_transportDecided && m_preferredTransport != QStringLiteral("pipe")) {
        if (!m_pendingCommand.isEmpty()) return;
        m_pendingCommand = trimmed;
        if (ensureSession()) dispatchPendingInput();
        return;
    }
    if (m_transportDecided && m_transportInteractive) {
        sendInput(trimmed + QLatin1Char('\r'));
        return;
    }
    static const QRegularExpression powershellEnvironmentPattern(
        QStringLiteral("^\\$env:([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cmdEnvironmentPattern(
        QStringLiteral("^set\\s+([A-Za-z_][A-Za-z0-9_]*)=(.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch environmentMatch = powershellEnvironmentPattern.match(trimmed);
    if (!environmentMatch.hasMatch()) environmentMatch = cmdEnvironmentPattern.match(trimmed);
    if (environmentMatch.hasMatch()) {
        const QString name = environmentMatch.captured(1).toUpper();
        QString value = environmentMatch.captured(2).trimmed();
        const bool remove = value.compare(QStringLiteral("$null"), Qt::CaseInsensitive) == 0 || value.isEmpty();
        if ((value.startsWith(QLatin1Char('\"')) && value.endsWith(QLatin1Char('\"')))
            || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))
            value = value.mid(1, value.size() - 2);
        appendVisible(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
        if (remove) {
            m_sessionEnvironment.remove(name);
            appendVisible(QStringLiteral("[会话] 已移除环境变量 %1\n").arg(name).toUtf8());
        } else {
            m_sessionEnvironment.insert(name, value);
            appendVisible(QStringLiteral("[会话] 已保存环境变量 %1（值不会写入日志）\n").arg(name).toUtf8());
        }
        if (m_process.state() != QProcess::NotRunning) {
            QString synchronizationCommand;
            if (QFileInfo(m_shellProgram).fileName().compare(QStringLiteral("cmd.exe"), Qt::CaseInsensitive) == 0) {
                synchronizationCommand = remove ? QStringLiteral("set \"%1=\"").arg(name)
                                                : QStringLiteral("set \"%1=%2\"").arg(name, value);
            } else {
                const QString quotedValue = QString(value).replace(QLatin1Char('\''), QStringLiteral("''"));
                synchronizationCommand = remove ? QStringLiteral("Remove-Item Env:%1 -ErrorAction SilentlyContinue").arg(name)
                                                : QStringLiteral("$env:%1='%2'").arg(name, quotedValue);
            }
            m_process.write("C\t" + synchronizationCommand.toUtf8().toBase64() + '\n');
        }
        m_statusText = QStringLiteral("会话环境已更新 · %1 个变量").arg(m_sessionEnvironment.size());
        writeLog(QStringLiteral("session.environment.updated"), QStringLiteral("name=%1 action=%2 count=%3")
            .arg(name, remove ? QStringLiteral("remove") : QStringLiteral("set"))
            .arg(m_sessionEnvironment.size()));
        emit statusChanged();
        return;
    }
    static const QRegularExpression changeDirectoryPattern(
        QStringLiteral("^(?:cd|Set-Location)(?:\\s+(.+))?$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch directoryMatch = changeDirectoryPattern.match(trimmed);
    if (directoryMatch.hasMatch()) {
        QString requested = directoryMatch.captured(1).trimmed();
        if ((requested.startsWith(QLatin1Char('\"')) && requested.endsWith(QLatin1Char('\"')))
            || (requested.startsWith(QLatin1Char('\'')) && requested.endsWith(QLatin1Char('\''))))
            requested = requested.mid(1, requested.size() - 2);
        const QString candidate = requested.isEmpty()
            ? m_sandboxPath : QDir(m_currentDirectory).absoluteFilePath(requested);
        const QString resolved = QDir::fromNativeSeparators(QFileInfo(candidate).canonicalFilePath());
        const QString root = QDir::fromNativeSeparators(QDir(m_sandboxPath).canonicalPath());
        const bool insideSandbox = !resolved.isEmpty()
            && (resolved.compare(root, Qt::CaseInsensitive) == 0
                || resolved.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive));
        appendVisible(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
        if (!insideSandbox || !QFileInfo(resolved).isDir()) {
            appendVisible(QStringLiteral("[会话] 目录不存在或超出沙盒范围\n").toUtf8());
            m_statusText = QStringLiteral("目录切换被拒绝");
        } else {
            m_currentDirectory = QDir(resolved).absolutePath();
            if (m_process.state() != QProcess::NotRunning) {
                QString synchronizationCommand;
                if (QFileInfo(m_shellProgram).fileName().compare(QStringLiteral("cmd.exe"), Qt::CaseInsensitive) == 0) {
                    synchronizationCommand = QStringLiteral("cd /d \"%1\"").arg(QDir::toNativeSeparators(m_currentDirectory));
                } else {
                    const QString quotedPath = QString(m_currentDirectory).replace(QLatin1Char('\''), QStringLiteral("''"));
                    synchronizationCommand = QStringLiteral("Set-Location -LiteralPath '%1'").arg(quotedPath);
                }
                m_process.write("C\t" + synchronizationCommand.toUtf8().toBase64() + '\n');
            }
            appendVisible(QStringLiteral("%1\n").arg(m_currentDirectory).toUtf8());
            m_statusText = QStringLiteral("工作目录已更新");
            emit currentDirectoryChanged();
        }
        emit statusChanged();
        return;
    }
    m_outputBytes = 0;
    m_runTimer.start();
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(trimmed.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
    writeLog(QStringLiteral("command.start"), QStringLiteral("length=%1 sha256=%2 shell=%3")
        .arg(trimmed.size()).arg(fingerprint, m_shellProgram));
    appendVisible(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
    m_commandRunning = true;
    m_commandMarker = "__SIDEBAR_DONE_" + QUuid::createUuid().toString(QUuid::Id128).toLatin1();
    m_pendingCommand = trimmed;
    emit runningChanged();
    m_statusText = QStringLiteral("正在持续 Shell 会话中执行");
    emit statusChanged();
    if (!ensureSession()) { finishCommand(QStringLiteral("start-failed"), -1); return; }
    dispatchPendingInput();
    if (m_commandTimeoutSeconds > 0) m_commandTimeout.start(m_commandTimeoutSeconds * 1000);
    m_elapsedPulse.start();
}

void TerminalManager::stop()
{
    if (m_process.state() == QProcess::NotRunning) return;
    writeLog(QStringLiteral("session.stop"), QStringLiteral("elapsedMs=%1").arg(elapsedMilliseconds()));
    requestClose(QStringLiteral("cancelled"));
}

void TerminalManager::openLogFile()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_logPath));
}

void TerminalManager::clear()
{
    m_screen->clear();
    m_output.clear();
    emit outputChanged();
}

void TerminalManager::appendOutput(const QByteArray &data)
{
    if (data.isEmpty()) return;
    m_outputBytes += data.size();
    m_protocolPending += data;
    if (!m_transportDecided) {
        const int newline = m_protocolPending.indexOf('\n');
        if (newline < 0) {
            if (m_protocolPending.size() > 4096) requestClose(QStringLiteral("start-failed"));
            return;
        }
        const QByteArray header = m_protocolPending.left(newline).trimmed();
        if (header != "__SIDEBAR_TRANSPORT_CONPTY__" && header != "__SIDEBAR_TRANSPORT_PIPE__") {
            m_statusText = QStringLiteral("Runner 传输协议不兼容，请更新 Runner");
            emit statusChanged(); requestClose(QStringLiteral("start-failed")); return;
        }
        m_protocolPending.remove(0, newline + 1);
        m_transportDecided = true;
        m_transportInteractive = header == "__SIDEBAR_TRANSPORT_CONPTY__";
        m_transportName = m_transportInteractive ? QStringLiteral("ConPTY · 原生交互") : QStringLiteral("兼容会话管道");
        m_statusText = m_transportInteractive ? QStringLiteral("原生传输已连接 · 请按 Shell 提示操作") : QStringLiteral("兼容 Shell 已就绪");
        m_startTimeout.stop();
        if (m_transportInteractive) m_elapsedPulse.start();
        if (m_transportInteractive)
            m_process.write("S\t" + QByteArray::number(m_columns) + '\t' + QByteArray::number(m_rows) + '\n');
        writeLog(QStringLiteral("transport.ready"), QStringLiteral("interactive=%1 columns=%2 rows=%3")
            .arg(m_transportInteractive).arg(m_columns).arg(m_rows));
        emit transportChanged(); emit statusChanged(); emit sessionChanged();
        dispatchPendingInput();
    }
    if (m_transportInteractive || m_commandMarker.isEmpty()) {
        const QByteArray visible = m_protocolPending;
        m_protocolPending.clear(); appendVisible(visible); return;
    }
    // Hold only a possible marker suffix; ordinary output is rendered immediately.
    // Matching is nonce-scoped to this one outstanding framed command.
    while (!m_protocolPending.isEmpty()) {
        const QByteArray prefix = m_commandMarker + '_';
        const int markerStart = m_protocolPending.indexOf(prefix);
        if (markerStart < 0) {
            int held = 0;
            for (int length = 1; length < prefix.size() && length <= m_protocolPending.size(); ++length)
                if (m_protocolPending.endsWith(prefix.left(length))) held = length;
            const int visibleSize = int(m_protocolPending.size()) - held;
            appendVisible(m_protocolPending.left(visibleSize));
            m_protocolPending.remove(0, visibleSize);
            return;
        }
        if (markerStart > 0) { appendVisible(m_protocolPending.left(markerStart)); m_protocolPending.remove(0, markerStart); }
        const int newline = m_protocolPending.indexOf('\n', prefix.size());
        if (newline < 0 && m_protocolPending.size() <= prefix.size() + 32) return;
        bool valid = false;
        const QByteArray number = newline >= 0 ? m_protocolPending.mid(prefix.size(), newline - prefix.size()).trimmed() : QByteArray();
        const int code = number.toInt(&valid);
        if (valid && !number.isEmpty() && QRegularExpression(QStringLiteral("^-?\\d+$")).match(QString::fromLatin1(number)).hasMatch()) {
            m_protocolPending.remove(0, newline + 1);
            finishCommand(code == 0 ? QStringLiteral("success") : QStringLiteral("failed"), code, true);
            appendVisible(m_protocolPending); m_protocolPending.clear(); return;
        }
        appendVisible(m_protocolPending.left(1)); m_protocolPending.remove(0, 1);
    }
}

void TerminalManager::appendVisible(const QByteArray &data)
{
    if (data.isEmpty()) return;
    m_screen->feed(data);
    QString decoded = stripTerminalSequences(m_utf8Decoder(data));
    decoded.replace(QStringLiteral("\r\r\n"), QStringLiteral("\r\n"));
    m_output += decoded;
    constexpr qsizetype maximumCharacters = 256 * 1024;
    if (m_output.size() > maximumCharacters) {
        qsizetype start = m_output.size() - maximumCharacters;
        if (m_output.at(start).isLowSurrogate()) ++start;
        m_output = QStringLiteral("[较早输出已截断]\n") + m_output.mid(start);
    }
    emit outputChanged();
}

QString TerminalManager::stripTerminalSequences(const QString &text)
{
    QString clean;
    clean.reserve(text.size());
    for (const QChar character : text) {
        const ushort value = character.unicode();
        switch (m_escapeState) {
        case EscapeState::Normal:
            if (value == 0x1b) m_escapeState = EscapeState::Escape;
            else if (value != 0x00) clean += character;
            break;
        case EscapeState::Escape:
            if (character == QLatin1Char('[')) m_escapeState = EscapeState::Csi;
            else if (character == QLatin1Char(']')) m_escapeState = EscapeState::Osc;
            else m_escapeState = EscapeState::Normal;
            break;
        case EscapeState::Csi:
            if (value >= 0x40 && value <= 0x7e) m_escapeState = EscapeState::Normal;
            break;
        case EscapeState::Osc:
            if (value == 0x07) m_escapeState = EscapeState::Normal;
            else if (value == 0x1b) m_escapeState = EscapeState::OscEscape;
            break;
        case EscapeState::OscEscape:
            m_escapeState = character == QLatin1Char('\\') ? EscapeState::Normal : EscapeState::Osc;
            break;
        }
    }
    return clean;
}

void TerminalManager::writeLog(const QString &event, const QString &detail)
{
    QLockFile lock(m_logPath + QStringLiteral(".lock"));
    lock.setStaleLockTime(10000);
    // Log contention must not freeze the GUI while the Runner writes its entry.
    if (!lock.tryLock(25)) return;
    if (QFileInfo(m_logPath).size() > 2 * 1024 * 1024) {
        QFile::remove(m_logPath + QStringLiteral(".1"));
        QFile::rename(m_logPath, m_logPath + QStringLiteral(".1"));
    }
    QFile file(m_logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    const QString line = QStringLiteral("%1 [host:%2] %3 %4\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
        .arg(QCoreApplication::applicationPid()).arg(event, detail);
    file.write(line.toUtf8());
}
