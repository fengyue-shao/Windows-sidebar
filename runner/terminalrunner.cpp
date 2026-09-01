#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>
#include <QStringConverter>
#include <Windows.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int commandIndex = arguments.indexOf(QStringLiteral("--command"));
    const int cwdIndex = arguments.indexOf(QStringLiteral("--cwd"));
    const int shellIndex = arguments.indexOf(QStringLiteral("--shell"));
    if (commandIndex < 0 || commandIndex + 1 >= arguments.size()
        || cwdIndex < 0 || cwdIndex + 1 >= arguments.size()
        || shellIndex < 0 || shellIndex + 1 >= arguments.size())
        return 2;

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        return 3;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.BasicLimitInformation.ActiveProcessLimit = 6;
    limits.ProcessMemoryLimit = 256ULL * 1024ULL * 1024ULL;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

    QProcess shell;
    const QString shellProgram = arguments.at(shellIndex + 1);
    const bool commandPrompt = QFileInfo(shellProgram).fileName().compare(QStringLiteral("cmd.exe"), Qt::CaseInsensitive) == 0;
    shell.setWorkingDirectory(arguments.at(cwdIndex + 1));
    QProcessEnvironment environment;
    const auto systemEnvironment = QProcessEnvironment::systemEnvironment();
    for (const QString &name : {QStringLiteral("SystemRoot"), QStringLiteral("WINDIR"),
                                QStringLiteral("PATH"), QStringLiteral("TEMP"), QStringLiteral("TMP")})
        environment.insert(name, systemEnvironment.value(name));
    environment.insert(QStringLiteral("SIDEBAR_RESTRICTED_TERMINAL"), QStringLiteral("1"));
    shell.setProcessEnvironment(environment);
    shell.setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(&shell, &QProcess::readyRead, [&shell, commandPrompt] {
        QTextStream output(stdout, QIODevice::WriteOnly);
        output.setEncoding(QStringConverter::Utf8);
        const QByteArray bytes = shell.readAll();
        output << (commandPrompt
            ? QString::fromUtf16(reinterpret_cast<const char16_t *>(bytes.constData()), bytes.size() / 2)
            : QString::fromUtf8(bytes)) << Qt::flush;
    });
    QObject::connect(&shell, &QProcess::finished, &app, [&app, job](int code) {
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
        CloseHandle(job);
        return 4;
    }
    HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                                 static_cast<DWORD>(shell.processId()));
    if (!process || !AssignProcessToJobObject(job, process)) {
        if (process) CloseHandle(process);
        shell.kill();
        shell.waitForFinished();
        CloseHandle(job);
        return 5;
    }
    CloseHandle(process);

    QTimer::singleShot(60000, &app, [&shell] {
        if (shell.state() != QProcess::NotRunning) {
            QTextStream(stderr) << "\n[runner] 命令超过 60 秒，已终止\n" << Qt::flush;
            shell.kill();
        }
    });
    return app.exec();
}
