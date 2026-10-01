#include "include/terminalmanager.h"
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
                const QString program = QProcess::splitCommand(commandLine).value(0);
                const QString base = QFileInfo(program).fileName().toLower();
                if (base == QStringLiteral("pwsh.exe") || base == QStringLiteral("powershell.exe") || base == QStringLiteral("cmd.exe"))
                    return {base, name};
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

TerminalManager::TerminalManager(QObject *parent) : QObject(parent)
{
    m_commandTimeout.setSingleShot(true);
    m_commandTimeout.setInterval(60000);
    connect(&m_commandTimeout, &QTimer::timeout, this, [this] {
        if (!m_commandRunning) return;
        writeLog(QStringLiteral("command.timeout"));
        stop();
    });
#ifdef Q_OS_WIN
    const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    m_conPtyAvailable = kernel
        && GetProcAddress(kernel, "CreatePseudoConsole")
        && GetProcAddress(kernel, "ResizePseudoConsole")
        && GetProcAddress(kernel, "ClosePseudoConsole");
#endif
    m_transportName = m_conPtyAvailable ? QStringLiteral("ConPTY 可用 · 会话管道")
                                        : QStringLiteral("兼容会话管道");
    m_sandboxPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/terminal-sandbox");
    QDir().mkpath(m_sandboxPath);
    m_sandboxPath = QDir(m_sandboxPath).absolutePath();
    m_currentDirectory = m_sandboxPath;
    const QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
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
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this] { appendOutput(m_process.readAll()); });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_commandTimeout.stop();
        if (m_commandRunning)
            appendOutput(QStringLiteral("\n[会话已终止] Runner 退出码 %1\n").arg(code).toUtf8());
        m_commandRunning = false;
        m_statusText = status == QProcess::NormalExit ? QStringLiteral("Shell 会话已关闭") : QStringLiteral("Shell 会话异常终止");
        writeLog(QStringLiteral("session.finished"), QStringLiteral("exitCode=%1 exitStatus=%2").arg(code).arg(static_cast<int>(status)));
        emit runningChanged();
        emit statusChanged();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        m_statusText = QStringLiteral("Runner 错误：") + m_process.errorString();
        appendOutput(QStringLiteral("\n[Runner 错误] %1\n").arg(m_process.errorString()).toUtf8());
        writeLog(QStringLiteral("runner.error"), QStringLiteral("type=%1 message=%2")
            .arg(static_cast<int>(error)).arg(m_process.errorString()));
        emit statusChanged();
    });
}

bool TerminalManager::ensureSession()
{
    if (m_process.state() != QProcess::NotRunning) return true;
    const QString runner = QCoreApplication::applicationDirPath() + QStringLiteral("/sidebar-terminal-runner.exe");
    QStringList arguments = {QStringLiteral("--session"), QStringLiteral("--cwd"), m_currentDirectory,
                             QStringLiteral("--shell"), m_shellProgram, QStringLiteral("--log"), m_logPath};
    for (auto iterator = m_sessionEnvironment.constBegin(); iterator != m_sessionEnvironment.constEnd(); ++iterator) {
        arguments += QStringLiteral("--env64");
        arguments += iterator.key() + QLatin1Char(':') + QString::fromLatin1(iterator.value().toUtf8().toBase64());
    }
    m_process.start(runner, arguments);
    if (!m_process.waitForStarted(5000)) {
        m_statusText = QStringLiteral("无法启动持续 Runner：") + m_process.errorString();
        emit statusChanged();
        return false;
    }
    writeLog(QStringLiteral("session.started"), QStringLiteral("shell=%1 cwd=%2").arg(m_shellProgram, m_currentDirectory));
    return true;
}

TerminalManager::~TerminalManager()
{
    m_commandTimeout.stop();
    m_process.kill();
    m_process.waitForFinished(3000);
}

void TerminalManager::resetSession()
{
    m_commandTimeout.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(3000);
    }
    m_commandRunning = false;
    m_sessionEnvironment.clear();
    m_currentDirectory = m_sandboxPath;
    m_utf8Decoder = QStringDecoder(QStringConverter::Utf8);
    m_escapeState = EscapeState::Normal;
    clear();
    m_statusText = QStringLiteral("会话已重置，下次执行时启动 Shell");
    emit currentDirectoryChanged();
    emit runningChanged();
    emit statusChanged();
}

QStringList TerminalManager::completionSuggestions(const QString &input) const
{
    const QString text = input;
    if (text.trimmed().isEmpty() || text.trimmed().startsWith(QLatin1Char('/')))
        return {};

    const int tokenStart = qMax(text.lastIndexOf(QLatin1Char(' ')), text.lastIndexOf(QLatin1Char('\t'))) + 1;
    const QString token = text.mid(tokenStart);
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
        results += value + (entry.isDir()
            ? QStringLiteral("\t目录\t沙盒中的目录")
            : QStringLiteral("\t文件\t沙盒中的文件"));
        if (results.size() >= maximumResults) break;
    }
    return results;
}

void TerminalManager::run(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty() || running()) return;
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
        appendOutput(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
        if (remove) {
            m_sessionEnvironment.remove(name);
            appendOutput(QStringLiteral("[会话] 已移除环境变量 %1\n").arg(name).toUtf8());
        } else {
            m_sessionEnvironment.insert(name, value);
            appendOutput(QStringLiteral("[会话] 已保存环境变量 %1（值不会写入日志）\n").arg(name).toUtf8());
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
        appendOutput(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
        if (!insideSandbox || !QFileInfo(resolved).isDir()) {
            appendOutput(QStringLiteral("[会话] 目录不存在或超出沙盒范围\n").toUtf8());
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
            appendOutput(QStringLiteral("%1\n").arg(m_currentDirectory).toUtf8());
            m_statusText = QStringLiteral("工作目录已更新");
            emit currentDirectoryChanged();
        }
        emit statusChanged();
        return;
    }
    m_utf8Decoder = QStringDecoder(QStringConverter::Utf8);
    m_escapeState = EscapeState::Normal;
    m_outputBytes = 0;
    m_runTimer.start();
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(trimmed.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
    writeLog(QStringLiteral("command.start"), QStringLiteral("length=%1 sha256=%2 shell=%3")
        .arg(trimmed.size()).arg(fingerprint, m_shellProgram));
    appendOutput(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
    if (!ensureSession()) return;
    m_commandRunning = true;
    emit runningChanged();
    m_statusText = QStringLiteral("正在持续 Shell 会话中执行");
    emit statusChanged();
    m_process.write("R\t" + trimmed.toUtf8().toBase64() + '\n');
    m_commandTimeout.start();
}

void TerminalManager::stop()
{
    if (!running()) return;
    m_statusText = QStringLiteral("正在终止命令树…");
    writeLog(QStringLiteral("command.stop"), QStringLiteral("elapsedMs=%1").arg(m_runTimer.elapsed()));
    emit statusChanged();
    m_process.kill();
}

void TerminalManager::openLogFile()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_logPath));
}

void TerminalManager::clear()
{
    if (m_output.isEmpty()) return;
    m_output.clear();
    emit outputChanged();
}

void TerminalManager::appendOutput(const QByteArray &data)
{
    m_outputBytes += data.size();
    QString decoded = stripTerminalSequences(m_utf8Decoder(data));
    decoded.replace(QStringLiteral("\r\r\n"), QStringLiteral("\r\n"));
    m_output += decoded;
    static const QRegularExpression completionMarker(QStringLiteral("(?:\\r?\\n)?__SIDEBAR_DONE_(-?\\d+)(?:\\r?\\n)?"));
    QRegularExpressionMatch marker;
    while ((marker = completionMarker.match(m_output)).hasMatch()) {
        const int code = marker.captured(1).toInt();
        m_output.remove(marker.capturedStart(), marker.capturedLength());
        m_output += QStringLiteral("\n[完成] 退出码 %1 · %2 ms\n").arg(code).arg(m_runTimer.elapsed());
        m_commandRunning = false;
        m_commandTimeout.stop();
        m_statusText = code == 0 ? QStringLiteral("命令已完成 · Shell 会话保持")
                                 : QStringLiteral("命令执行结束，退出码 %1").arg(code);
        writeLog(QStringLiteral("command.finished"), QStringLiteral("exitCode=%1 elapsedMs=%2 outputBytes=%3")
            .arg(code).arg(m_runTimer.elapsed()).arg(m_outputBytes));
        emit runningChanged();
        emit statusChanged();
        emit commandCompleted(code,m_runTimer.elapsed());
    }
    constexpr qsizetype maximumCharacters = 256 * 1024;
    if (m_output.size() > maximumCharacters)
        m_output = QStringLiteral("[较早输出已截断]\n") + m_output.right(maximumCharacters);
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
    if (!lock.tryLock(1500)) return;
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
