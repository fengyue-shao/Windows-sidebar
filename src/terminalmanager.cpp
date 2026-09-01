#include "include/terminalmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

TerminalManager::TerminalManager(QObject *parent) : QObject(parent)
{
    m_sandboxPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/terminal-sandbox");
    QDir().mkpath(m_sandboxPath);
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this] { appendOutput(m_process.readAll()); });
    connect(&m_process, &QProcess::stateChanged, this, &TerminalManager::runningChanged);
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        appendOutput(QStringLiteral("\n[完成] 退出码 %1 · %2\n")
            .arg(code).arg(status == QProcess::NormalExit ? QStringLiteral("正常退出") : QStringLiteral("进程中止")).toUtf8());
        m_statusText = code == 0 ? QStringLiteral("命令已完成") : QStringLiteral("命令执行结束，退出码 %1").arg(code);
        emit statusChanged();
    });
}

void TerminalManager::run(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty() || running()) return;
    appendOutput(QStringLiteral("\nPS sandbox> %1\n").arg(trimmed).toUtf8());
    m_statusText = QStringLiteral("正在独立 Runner 中执行");
    emit statusChanged();
    const QString runner = QCoreApplication::applicationDirPath() + QStringLiteral("/sidebar-terminal-runner.exe");
    m_process.start(runner, {QStringLiteral("--cwd"), m_sandboxPath,
                             QStringLiteral("--command"), trimmed});
}

void TerminalManager::stop()
{
    if (!running()) return;
    m_statusText = QStringLiteral("正在终止命令树…");
    emit statusChanged();
    m_process.kill();
}

void TerminalManager::clear()
{
    if (m_output.isEmpty()) return;
    m_output.clear();
    emit outputChanged();
}

void TerminalManager::appendOutput(const QByteArray &data)
{
    m_output += QString::fromLocal8Bit(data);
    constexpr qsizetype maximumCharacters = 256 * 1024;
    if (m_output.size() > maximumCharacters)
        m_output = QStringLiteral("[较早输出已截断]\n") + m_output.right(maximumCharacters);
    emit outputChanged();
}
