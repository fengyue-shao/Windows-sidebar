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
    m_sandboxPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/terminal-sandbox");
    QDir().mkpath(m_sandboxPath);
    const QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    QDir().mkpath(logDirectory);
    m_logPath = logDirectory + QStringLiteral("/terminal-plugin.log");
    const auto shell = detectDefaultShell();
    m_shellProgram = shell.first;
    m_shellName = shell.second + QStringLiteral(" · ") + QFileInfo(shell.first).fileName();
    writeLog(QStringLiteral("manager.init"), QStringLiteral("shell=%1 program=%2 sandbox=%3")
        .arg(m_shellName, m_shellProgram, m_sandboxPath));
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this] { appendOutput(m_process.readAll()); });
    connect(&m_process, &QProcess::stateChanged, this, &TerminalManager::runningChanged);
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        appendOutput(QStringLiteral("\n[完成] 退出码 %1 · %2\n")
            .arg(code).arg(status == QProcess::NormalExit ? QStringLiteral("正常退出") : QStringLiteral("进程中止")).toUtf8());
        m_statusText = code == 0 ? QStringLiteral("命令已完成") : QStringLiteral("命令执行结束，退出码 %1").arg(code);
        writeLog(QStringLiteral("command.finished"), QStringLiteral("exitCode=%1 exitStatus=%2 elapsedMs=%3 outputBytes=%4")
            .arg(code).arg(static_cast<int>(status)).arg(m_runTimer.isValid() ? m_runTimer.elapsed() : -1).arg(m_outputBytes));
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

void TerminalManager::run(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty() || running()) return;
    m_utf8Decoder = QStringDecoder(QStringConverter::Utf8);
    m_outputBytes = 0;
    m_runTimer.start();
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(trimmed.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
    writeLog(QStringLiteral("command.start"), QStringLiteral("length=%1 sha256=%2 shell=%3")
        .arg(trimmed.size()).arg(fingerprint, m_shellProgram));
    appendOutput(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
    m_statusText = QStringLiteral("正在独立 Runner 中执行");
    emit statusChanged();
    const QString runner = QCoreApplication::applicationDirPath() + QStringLiteral("/sidebar-terminal-runner.exe");
    m_process.start(runner, {QStringLiteral("--cwd"), m_sandboxPath,
                             QStringLiteral("--shell"), m_shellProgram,
                             QStringLiteral("--log"), m_logPath,
                             QStringLiteral("--command"), trimmed});
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
    m_output += m_utf8Decoder(data);
    constexpr qsizetype maximumCharacters = 256 * 1024;
    if (m_output.size() > maximumCharacters)
        m_output = QStringLiteral("[较早输出已截断]\n") + m_output.right(maximumCharacters);
    emit outputChanged();
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
