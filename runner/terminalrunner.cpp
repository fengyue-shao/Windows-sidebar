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
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <thread>

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
    const bool sessionMode = arguments.contains(QStringLiteral("--session"));
    if ((!sessionMode && (commandIndex < 0 || commandIndex + 1 >= arguments.size()))
        || cwdIndex < 0 || cwdIndex + 1 >= arguments.size()
        || shellIndex < 0 || shellIndex + 1 >= arguments.size())
        return 2;
    const QString logPath = logIndex >= 0 && logIndex + 1 < arguments.size() ? arguments.at(logIndex + 1) : QString();

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);

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
    writeRunnerLog(logPath, QStringLiteral("runner.start"), QStringLiteral("shell=%1 mode=%2 cwd=%3 session=%4")
        .arg(shellProgram, commandPrompt ? QStringLiteral("cmd-utf16") : QStringLiteral("powershell-utf8"), arguments.at(cwdIndex + 1))
        .arg(sessionMode));
    shell.setWorkingDirectory(arguments.at(cwdIndex + 1));
    QProcessEnvironment environment;
    const auto systemEnvironment = QProcessEnvironment::systemEnvironment();
    const QStringList inheritedEnvironment = {
        QStringLiteral("SystemRoot"), QStringLiteral("WINDIR"), QStringLiteral("SystemDrive"),
        QStringLiteral("ComSpec"), QStringLiteral("PATH"), QStringLiteral("PATHEXT"),
        QStringLiteral("TEMP"), QStringLiteral("TMP"), QStringLiteral("USERPROFILE"),
        QStringLiteral("HOMEDRIVE"), QStringLiteral("HOMEPATH"), QStringLiteral("USERNAME"),
        QStringLiteral("USERDOMAIN"), QStringLiteral("APPDATA"), QStringLiteral("LOCALAPPDATA"),
        QStringLiteral("ProgramData"), QStringLiteral("ProgramFiles"), QStringLiteral("ProgramFiles(x86)"),
        QStringLiteral("CommonProgramFiles"), QStringLiteral("CommonProgramFiles(x86)"),
        QStringLiteral("PROCESSOR_ARCHITECTURE"), QStringLiteral("NUMBER_OF_PROCESSORS"),
        QStringLiteral("PSModulePath")
    };
    for (const QString &name : inheritedEnvironment) {
        const QString value = systemEnvironment.value(name);
        if (!value.isEmpty()) environment.insert(name, value);
    }
    environment.insert(QStringLiteral("SIDEBAR_RESTRICTED_TERMINAL"), QStringLiteral("1"));
    environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    for (int index = 0; index + 1 < arguments.size(); ++index) {
        if (arguments.at(index) != QStringLiteral("--env64")) continue;
        const QString encoded = arguments.at(index + 1);
        const int separator = encoded.indexOf(QLatin1Char(':'));
        if (separator <= 0) continue;
        const QString name = encoded.left(separator);
        const QString value = QString::fromUtf8(QByteArray::fromBase64(encoded.mid(separator + 1).toLatin1()));
        environment.insert(name, value);
    }
    shell.setProcessEnvironment(environment);
    shell.setProcessChannelMode(QProcess::MergedChannels);

    QStringDecoder streamDecoder(commandPrompt ? QStringConverter::Utf16LE : QStringConverter::Utf8);
    QObject::connect(&shell, &QProcess::readyRead, [&shell, &streamDecoder] {
        const QByteArray bytes = shell.readAll();
        const QString decoded = streamDecoder(bytes);
        const QByteArray utf8 = decoded.toUtf8();
        fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stdout);
        fflush(stdout);
    });
    QObject::connect(&shell, &QProcess::finished, &app, [&app, job, logPath](int code, QProcess::ExitStatus status) {
        writeRunnerLog(logPath, QStringLiteral("shell.finished"), QStringLiteral("exitCode=%1 exitStatus=%2").arg(code).arg(static_cast<int>(status)));
        CloseHandle(job);
        app.exit(code);
    });
    if (sessionMode && commandPrompt) {
        shell.start(shellProgram, {QStringLiteral("/U"), QStringLiteral("/D"), QStringLiteral("/Q"), QStringLiteral("/K")});
    } else if (sessionMode) {
        shell.start(shellProgram, {QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
            QStringLiteral("-ExecutionPolicy"), QStringLiteral("Restricted"), QStringLiteral("-NoExit"), QStringLiteral("-Command"), QStringLiteral("-")});
    } else if (commandPrompt) {
        shell.start(shellProgram, {QStringLiteral("/U"), QStringLiteral("/D"), QStringLiteral("/S"), QStringLiteral("/C"),
            QStringLiteral("chcp 65001>nul & ") + arguments.at(commandIndex + 1)});
    } else {
        shell.start(shellProgram, {
            QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
            QStringLiteral("-ExecutionPolicy"), QStringLiteral("Restricted"), QStringLiteral("-Command"),
            QStringLiteral("[Console]::InputEncoding=[Text.UTF8Encoding]::new($false); [Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); $OutputEncoding=[Text.UTF8Encoding]::new($false); if (Get-Variable PSStyle -ErrorAction SilentlyContinue) { $PSStyle.OutputRendering='PlainText' }; $ExecutionContext.SessionState.LanguageMode='ConstrainedLanguage'; ")
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

    if (sessionMode) {
        if (commandPrompt)
            shell.write("chcp 65001>nul\r\n");
        else
            shell.write("[Console]::InputEncoding=[Text.UTF8Encoding]::new($false); [Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); $OutputEncoding=[Text.UTF8Encoding]::new($false); if (Get-Variable PSStyle -ErrorAction SilentlyContinue) { $PSStyle.OutputRendering='PlainText' }; $ExecutionContext.SessionState.LanguageMode='ConstrainedLanguage'\n");
        std::thread([&shell, commandPrompt] {
            QByteArray line;
            while (true) {
                const int character = getchar();
                if (character == EOF) break;
                if (character != '\n') { line += static_cast<char>(character); continue; }
                QByteArray request = line.trimmed();
                line.clear();
                const bool controlMessage = request.startsWith("C\t");
                if (controlMessage || request.startsWith("R\t")) request.remove(0, 2);
                const QString command = QString::fromUtf8(QByteArray::fromBase64(request));
                QMetaObject::invokeMethod(&shell, [command, commandPrompt, controlMessage, &shell] {
                    const QString wrapped = controlMessage
                        ? command + (commandPrompt ? QStringLiteral("\r\n") : QStringLiteral("\n"))
                        : (commandPrompt
                            ? command + QStringLiteral(" & echo __SIDEBAR_DONE_%errorlevel%\r\n")
                            : QStringLiteral("%1; $__sidebarCode = if ($?) { 0 } else { 1 }; Write-Output \"__SIDEBAR_DONE_$__sidebarCode\"\n").arg(command));
                    shell.write(commandPrompt ? wrapped.toUtf8() : wrapped.toLocal8Bit());
                }, Qt::QueuedConnection);
            }
            QMetaObject::invokeMethod(&shell, [&shell] { shell.closeWriteChannel(); }, Qt::QueuedConnection);
        }).detach();
        return app.exec();
    }

    QTimer::singleShot(60000, &app, [&shell, logPath] {
        if (shell.state() != QProcess::NotRunning) {
            const QByteArray message = QStringLiteral("\n[runner] 命令超过 60 秒，已终止\n").toUtf8();
            fwrite(message.constData(), 1, static_cast<size_t>(message.size()), stderr);
            fflush(stderr);
            writeRunnerLog(logPath, QStringLiteral("command.timeout"));
            shell.kill();
        }
    });
    return app.exec();
}
