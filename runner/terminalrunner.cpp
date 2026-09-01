#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QLockFile>
#include <QTextStream>
#include <QTimer>
#include <QStringConverter>
#include <Windows.h>

namespace {
void writeRunnerLog(const QString &path, const QString &event, const QString &detail = {})
{
    if (path.isEmpty()) return;
    QLockFile lock(path + QStringLiteral(".lock"));
    lock.setStaleLockTime(10000);
    if (!lock.tryLock(1500)) return;
    if (QFileInfo(path).size() > 2 * 1024 * 1024) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    file.write(QStringLiteral("%1 [runner:%2] %3 %4\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
        .arg(QCoreApplication::applicationPid()).arg(event, detail).toUtf8());
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int commandIndex = arguments.indexOf(QStringLiteral("--command"));
    const int cwdIndex = arguments.indexOf(QStringLiteral("--cwd"));
    const int shellIndex = arguments.indexOf(QStringLiteral("--shell"));
    const int logIndex = arguments.indexOf(QStringLiteral("--log"));
    if (commandIndex < 0 || commandIndex + 1 >= arguments.size()
        || cwdIndex < 0 || cwdIndex + 1 >= arguments.size()
        || shellIndex < 0 || shellIndex + 1 >= arguments.size())
        return 2;
    const QString logPath = logIndex >= 0 && logIndex + 1 < arguments.size() ? arguments.at(logIndex + 1) : QString();

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        writeRunnerLog(logPath, QStringLiteral("job.create.failed"), QStringLiteral("winError=%1").arg(GetLastError()));
        return 3;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.BasicLimitInformation.ActiveProcessLimit = 6;
    limits.ProcessMemoryLimit = 256ULL * 1024ULL * 1024ULL;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        writeRunnerLog(logPath, QStringLiteral("job.limit.failed"), QStringLiteral("winError=%1").arg(GetLastError()));

    QProcess shell;
    const QString shellProgram = arguments.at(shellIndex + 1);
    const bool commandPrompt = QFileInfo(shellProgram).fileName().compare(QStringLiteral("cmd.exe"), Qt::CaseInsensitive) == 0;
    writeRunnerLog(logPath, QStringLiteral("runner.start"), QStringLiteral("shell=%1 mode=%2 cwd=%3")
        .arg(shellProgram, commandPrompt ? QStringLiteral("cmd-utf16") : QStringLiteral("powershell-utf8"), arguments.at(cwdIndex + 1)));
    shell.setWorkingDirectory(arguments.at(cwdIndex + 1));
    QProcessEnvironment environment;
    const auto systemEnvironment = QProcessEnvironment::systemEnvironment();
    for (const QString &name : {QStringLiteral("SystemRoot"), QStringLiteral("WINDIR"),
                                QStringLiteral("PATH"), QStringLiteral("TEMP"), QStringLiteral("TMP")})
        environment.insert(name, systemEnvironment.value(name));
    environment.insert(QStringLiteral("SIDEBAR_RESTRICTED_TERMINAL"), QStringLiteral("1"));
    shell.setProcessEnvironment(environment);
    shell.setProcessChannelMode(QProcess::MergedChannels);

    QStringDecoder streamDecoder(commandPrompt ? QStringConverter::Utf16LE : QStringConverter::Utf8);
    QObject::connect(&shell, &QProcess::readyRead, [&shell, &streamDecoder] {
        QTextStream output(stdout, QIODevice::WriteOnly);
        output.setEncoding(QStringConverter::Utf8);
        const QByteArray bytes = shell.readAll();
        output << streamDecoder(bytes) << Qt::flush;
    });
    QObject::connect(&shell, &QProcess::finished, &app, [&app, job, logPath](int code, QProcess::ExitStatus status) {
        writeRunnerLog(logPath, QStringLiteral("shell.finished"), QStringLiteral("exitCode=%1 exitStatus=%2").arg(code).arg(static_cast<int>(status)));
        CloseHandle(job);
        app.exit(code);
    });
    if (commandPrompt) {
        shell.start(shellProgram, {QStringLiteral("/U"), QStringLiteral("/D"), QStringLiteral("/S"), QStringLiteral("/C"),
            QStringLiteral("chcp 65001>nul & ") + arguments.at(commandIndex + 1)});
    } else {
        shell.start(shellProgram, {
            QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
            QStringLiteral("-ExecutionPolicy"), QStringLiteral("Restricted"), QStringLiteral("-Command"),
            QStringLiteral("[Console]::InputEncoding=[Text.UTF8Encoding]::new($false); [Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); $OutputEncoding=[Text.UTF8Encoding]::new($false); $ExecutionContext.SessionState.LanguageMode='ConstrainedLanguage'; ")
                + arguments.at(commandIndex + 1)
        });
    }
    if (!shell.waitForStarted(5000)) {
        writeRunnerLog(logPath, QStringLiteral("shell.start.failed"), shell.errorString());
        CloseHandle(job);
        return 4;
    }
    HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                                 static_cast<DWORD>(shell.processId()));
    if (!process || !AssignProcessToJobObject(job, process)) {
        writeRunnerLog(logPath, QStringLiteral("job.assign.failed"), QStringLiteral("winError=%1").arg(GetLastError()));
        if (process) CloseHandle(process);
        shell.kill();
        shell.waitForFinished();
        CloseHandle(job);
        return 5;
    }
    CloseHandle(process);

    QTimer::singleShot(60000, &app, [&shell, logPath] {
        if (shell.state() != QProcess::NotRunning) {
            QTextStream errorOutput(stderr);
            errorOutput.setEncoding(QStringConverter::Utf8);
            errorOutput << "\n[runner] 命令超过 60 秒，已终止\n" << Qt::flush;
            writeRunnerLog(logPath, QStringLiteral("command.timeout"));
            shell.kill();
        }
    });
    return app.exec();
}
