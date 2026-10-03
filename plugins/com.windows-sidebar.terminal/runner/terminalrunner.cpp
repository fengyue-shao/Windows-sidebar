#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QLockFile>
#include <QTextStream>
#include <QTimer>
#include <QStringConverter>
#include <QUuid>
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <vector>

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

class WinHandle final
{
public:
    explicit WinHandle(HANDLE value = nullptr) : m_value(value) {}
    ~WinHandle() { reset(); }
    WinHandle(const WinHandle &) = delete;
    WinHandle &operator=(const WinHandle &) = delete;
    HANDLE get() const { return m_value; }
    void reset(HANDLE value = nullptr)
    {
        if (m_value && m_value != INVALID_HANDLE_VALUE) CloseHandle(m_value);
        m_value = value;
    }
private:
    HANDLE m_value;
};

struct Request
{
    enum Kind { Invalid, Command, Control, Input, Resize, Close } kind = Invalid;
    QString data;
    QString nonce;
    int columns = 0;
    int rows = 0;
};

Request parseRequest(const QByteArray &line)
{
    Request request;
    const auto parts = line.split('\t');
    if (parts.size() == 1 && parts.first() == "X") {
        request.kind = Request::Close;
        return request;
    }
    if (parts.size() == 3 && parts.first() == "S") {
        bool columnsOk = false, rowsOk = false;
        request.columns = parts.at(1).toInt(&columnsOk);
        request.rows = parts.at(2).toInt(&rowsOk);
        if (columnsOk && rowsOk && request.columns >= 20 && request.columns <= 400
            && request.rows >= 5 && request.rows <= 200) request.kind = Request::Resize;
        return request;
    }
    QByteArray encoded;
    if (parts.size() == 3 && parts.first() == "R") {
        const QByteArray nonce = parts.at(1);
        if (nonce.isEmpty() || nonce.size() > 64) return request;
        for (const char character : nonce) {
            if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z')
                  || (character >= 'A' && character <= 'Z') || character == '_')) return request;
        }
        request.nonce = QString::fromLatin1(nonce);
        request.kind = Request::Command;
        encoded = parts.at(2);
    } else if (parts.size() == 2 && (parts.first() == "R" || parts.first() == "C" || parts.first() == "I")) {
        request.kind = parts.first() == "R" ? Request::Command
            : parts.first() == "C" ? Request::Control : Request::Input;
        encoded = parts.at(1);
    } else if (parts.size() == 1) {
        // Compatibility with the original base64-only command protocol.
        request.kind = Request::Command;
        encoded = parts.first();
    } else {
        return request;
    }
    const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() > 128 * 1024) return {};
    request.data = QString::fromUtf8(decoded.decoded);
    if (request.data.contains(QChar(u'\0'))) return {};
    return request;
}

QString wrappedCommand(const Request &request, bool commandPrompt)
{
    const QString newline = commandPrompt ? QStringLiteral("\r\n") : QStringLiteral("\r");
    if (request.kind == Request::Input) return request.data;
    if (request.kind == Request::Control) return request.data + newline;
    const QString marker = request.nonce.isEmpty() ? QStringLiteral("__SIDEBAR_DONE_")
        : QStringLiteral("__SIDEBAR_DONE_%1_").arg(request.nonce);
    if (commandPrompt)
        return request.data + QStringLiteral("\r\necho ") + marker + QStringLiteral("%errorlevel%\r\n");
    return QStringLiteral("$global:LASTEXITCODE=0; %1; $__sidebarCode = if ($?) { 0 } elseif ($global:LASTEXITCODE -ne 0) { $global:LASTEXITCODE } else { 1 }; Write-Output \"%2$__sidebarCode\"\r")
        .arg(request.data, marker);
}

// The host always supplies an anonymous stdin pipe. Polling its readiness avoids
// detaching a reader that could later access a destroyed QProcess or application.
void readRequests(const std::atomic_bool &closing, const std::function<bool(const Request &)> &receive)
{
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    QByteArray line;
    bool oversized = false;
    char buffer[4096];
    while (!closing.load()) {
        if (GetFileType(input) == FILE_TYPE_PIPE) {
            DWORD available = 0;
            if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) break;
            if (available == 0) { Sleep(20); continue; }
        }
        DWORD count = 0;
        if (!ReadFile(input, buffer, sizeof(buffer), &count, nullptr) || count == 0) break;
        for (DWORD index = 0; index < count; ++index) {
            const char character = buffer[index];
            if (character == '\n') {
                if (!oversized && !line.isEmpty()) {
                    if (line.endsWith('\r')) line.chop(1);
                    if (!receive(parseRequest(line))) return;
                }
                line.clear();
                oversized = false;
            } else if (!oversized) {
                if (line.size() >= 192 * 1024) { line.clear(); oversized = true; }
                else line += character;
            }
        }
    }
}

bool writeAll(HANDLE handle, const QByteArray &bytes)
{
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        DWORD count = 0;
        const DWORD remaining = static_cast<DWORD>(qMin<qsizetype>(bytes.size() - offset, 65536));
        if (!WriteFile(handle, bytes.constData() + offset, remaining, &count, nullptr) || count == 0) return false;
        offset += count;
    }
    return true;
}

QString quoteWindowsArgument(const QString &argument)
{
    QString result = QStringLiteral("\"");
    int backslashes = 0;
    for (QChar character : argument) {
        if (character == QLatin1Char('\\')) { ++backslashes; continue; }
        if (character == QLatin1Char('"')) result += QString(backslashes * 2 + 1, QLatin1Char('\\'));
        else result += QString(backslashes, QLatin1Char('\\'));
        backslashes = 0;
        result += character;
    }
    return result + QString(backslashes * 2, QLatin1Char('\\')) + QLatin1Char('"');
}

std::wstring environmentBlock(const QProcessEnvironment &environment)
{
    QStringList names = environment.keys();
    names.sort(Qt::CaseInsensitive);
    std::wstring block;
    for (const QString &name : names) {
        const QString item = name + QLatin1Char('=') + environment.value(name);
        block.append(reinterpret_cast<const wchar_t *>(item.utf16()), static_cast<size_t>(item.size()));
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

std::optional<int> runPseudoConsole(const QString &shellProgram, bool commandPrompt, const QString &cwd,
                                   QProcessEnvironment environment, HANDLE job, const QString &logPath,
                                   int columns, int rows)
{
    using CreateFn = HRESULT (WINAPI *)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
    using ResizeFn = HRESULT (WINAPI *)(HPCON, COORD);
    using CloseFn = void (WINAPI *)(HPCON);
    const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    const auto createConsole = reinterpret_cast<CreateFn>(GetProcAddress(kernel, "CreatePseudoConsole"));
    const auto resizeConsole = reinterpret_cast<ResizeFn>(GetProcAddress(kernel, "ResizePseudoConsole"));
    const auto closeConsole = reinterpret_cast<CloseFn>(GetProcAddress(kernel, "ClosePseudoConsole"));
    if (!createConsole || !resizeConsole || !closeConsole) {
        writeRunnerLog(logPath, QStringLiteral("conpty.unavailable"));
        return std::nullopt;
    }
    HANDLE rawInputRead = nullptr, rawInputWrite = nullptr, rawOutputRead = nullptr, rawOutputWrite = nullptr;
    if (!CreatePipe(&rawInputRead, &rawInputWrite, nullptr, 0)) return std::nullopt;
    WinHandle inputRead(rawInputRead), inputWrite(rawInputWrite);
    if (!CreatePipe(&rawOutputRead, &rawOutputWrite, nullptr, 0)) return std::nullopt;
    WinHandle outputRead(rawOutputRead), outputWrite(rawOutputWrite);
    HPCON console = nullptr;
    const HRESULT created = createConsole({static_cast<SHORT>(columns), static_cast<SHORT>(rows)},
                                           inputRead.get(), outputWrite.get(), 0, &console);
    if (FAILED(created)) {
        writeRunnerLog(logPath, QStringLiteral("conpty.create.failed"), QStringLiteral("hresult=%1").arg(static_cast<quint32>(created)));
        return std::nullopt;
    }

    // Start draining before process creation; ClosePseudoConsole may itself emit
    // output even when child startup fails. The output thread remains alive until
    // the console closes its pipe, including on pre-24H2 Windows versions.
    std::atomic_bool closing{false};
    std::atomic_bool forwardOutput{false};
    WinHandle shellReadyEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    WinHandle forwardEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!shellReadyEvent.get() || !forwardEvent.get()) {
        outputWrite.reset();
        outputRead.reset();
        closeConsole(console);
        return std::nullopt;
    }
    std::thread outputThread([&] {
        char buffer[8192];
        bool startupComplete = false;
        while (true) {
            DWORD count = 0;
            if (!ReadFile(outputRead.get(), buffer, sizeof(buffer), &count, nullptr)) {
                if (GetLastError() == ERROR_OPERATION_ABORTED && closing.load()) continue;
                break;
            }
            if (count == 0) break;
            QByteArray bytes(buffer, static_cast<qsizetype>(count));
            if (!startupComplete && !closing.load()) {
                // First output proves attachment, not command readiness. Publish
                // prompts and queries immediately; waiting for an injected shell
                // marker hides trust/password prompts and can deadlock startup.
                SetEvent(shellReadyEvent.get());
                // The negotiation header must be the very first stdout line.
                WaitForSingleObject(forwardEvent.get(), INFINITE);
                startupComplete = true;
            }
            if (forwardOutput.load()) {
                if (!writeAll(GetStdHandle(STD_OUTPUT_HANDLE), bytes))
                    forwardOutput.store(false); // Keep draining when the host stops reading.
            }
        }
    });
    auto closeAndDrain = [&](bool preserveOutput = false) {
        closing.store(true);
        if (!preserveOutput) forwardOutput.store(false);
        SetEvent(forwardEvent.get());
        // Unblock a possible write to a disappearing host; the reader continues
        // draining ConPTY instead of forwarding bytes during teardown.
        CancelSynchronousIo(outputThread.native_handle());
        inputRead.reset();
        outputWrite.reset();
        closeConsole(console);
        outputThread.join();
    };

    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributes(attributeBytes);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    // Even with bInheritHandles=FALSE, Windows can duplicate redirected parent
    // standard handles into a console child unless STARTF_USESTDHANDLES is set.
    // The Runner's handles carry its host protocol, not shell input/output;
    // leave these NULL so the pseudoconsole supplies the child's own handles.
    startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullptr;
    startup.StartupInfo.hStdOutput = nullptr;
    startup.StartupInfo.hStdError = nullptr;
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (attributeBytes == 0 || !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes)) {
        closeAndDrain();
        return std::nullopt;
    }
    const bool updated = UpdateProcThreadAttribute(startup.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console, sizeof(console), nullptr, nullptr);
    if (!updated) {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        closeAndDrain();
        return std::nullopt;
    }
    environment.remove(QStringLiteral("NO_COLOR"));
    environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    environment.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    environment.insert(QStringLiteral("SIDEBAR_TERMINAL_TRANSPORT"), QStringLiteral("conpty"));
    const QString initialization = QStringLiteral("[Console]::InputEncoding=[Text.UTF8Encoding]::new($false); "
        "[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); $OutputEncoding=[Text.UTF8Encoding]::new($false); "
        "$ExecutionContext.SessionState.LanguageMode='ConstrainedLanguage'");
    const QStringList childArguments = commandPrompt
        ? QStringList{QStringLiteral("/D"), QStringLiteral("/K"), QStringLiteral("chcp 65001>nul")}
        : QStringList{QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"),
                      QStringLiteral("Restricted"), QStringLiteral("-NoExit"), QStringLiteral("-Command"), initialization};
    const QString nativeProgram = QDir::toNativeSeparators(shellProgram);
    const QString nativeDirectory = QDir::toNativeSeparators(cwd);
    QString commandLine = quoteWindowsArgument(nativeProgram);
    if (commandPrompt)
        commandLine += QStringLiteral(" /D /K chcp 65001>nul");
    else
        for (const QString &argument : childArguments) commandLine += QLatin1Char(' ') + quoteWindowsArgument(argument);
    std::wstring mutableCommand = commandLine.toStdWString();
    std::wstring childEnvironment = environmentBlock(environment);
    PROCESS_INFORMATION child{};
    const BOOL started = CreateProcessW(reinterpret_cast<LPCWSTR>(nativeProgram.utf16()), mutableCommand.data(),
        nullptr, nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
        childEnvironment.data(), reinterpret_cast<LPCWSTR>(nativeDirectory.utf16()), &startup.StartupInfo, &child);
    const DWORD startError = started ? ERROR_SUCCESS : GetLastError();
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    if (!started) {
        writeRunnerLog(logPath, QStringLiteral("conpty.shell.start.failed"), QStringLiteral("winError=%1").arg(startError));
        closeAndDrain();
        return std::nullopt;
    }
    WinHandle childProcess(child.hProcess), childThread(child.hThread);
    inputRead.reset();
    outputWrite.reset();
    if (!AssignProcessToJobObject(job, childProcess.get())) {
        writeRunnerLog(logPath, QStringLiteral("conpty.job.assign.failed"), QStringLiteral("winError=%1").arg(GetLastError()));
        TerminateProcess(childProcess.get(), 5);
        WaitForSingleObject(childProcess.get(), 5000);
        closeAndDrain();
        return std::nullopt;
    }
    if (ResumeThread(childThread.get()) == static_cast<DWORD>(-1)) {
        writeRunnerLog(logPath, QStringLiteral("conpty.shell.resume.failed"), QStringLiteral("winError=%1").arg(GetLastError()));
        TerminateJobObject(job, 5);
        closeAndDrain();
        return 5;
    }
    const HANDLE startupWaitHandles[] = {shellReadyEvent.get(), childProcess.get()};
    if (WaitForMultipleObjects(2, startupWaitHandles, FALSE, 5000) != WAIT_OBJECT_0) {
        writeRunnerLog(logPath, QStringLiteral("conpty.shell.ready.failed"));
        TerminateJobObject(job, 5);
        closeAndDrain();
        return std::nullopt;
    }
    if (!writeAll(GetStdHandle(STD_OUTPUT_HANDLE), "__SIDEBAR_TRANSPORT_CONPTY__\n")) {
        TerminateJobObject(job, 5);
        closeAndDrain();
        return 5;
    }
    forwardOutput.store(true);
    SetEvent(forwardEvent.get());
    WinHandle stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stopEvent.get()) {
        TerminateJobObject(job, 5);
        closeAndDrain();
        return 5;
    }
    writeRunnerLog(logPath, QStringLiteral("conpty.ready"), QStringLiteral("columns=%1 rows=%2").arg(columns).arg(rows));
    std::thread inputThread([&] {
        readRequests(closing, [&](const Request &request) {
            if (closing.load()) return false;
            if (request.kind == Request::Close) return false;
            if (request.kind == Request::Resize) {
                const HRESULT resized = resizeConsole(console, {static_cast<SHORT>(request.columns), static_cast<SHORT>(request.rows)});
                if (FAILED(resized)) writeRunnerLog(logPath, QStringLiteral("conpty.resize.failed"),
                    QStringLiteral("hresult=%1").arg(static_cast<quint32>(resized)));
            } else if (request.kind == Request::Input || request.kind == Request::Command || request.kind == Request::Control) {
                const QByteArray bytes = wrappedCommand(request, commandPrompt).toUtf8();
                writeRunnerLog(logPath, QStringLiteral("conpty.input"),
                    QStringLiteral("kind=%1 bytes=%2").arg(static_cast<int>(request.kind)).arg(bytes.size()));
                if (!writeAll(inputWrite.get(), bytes)) {
                    writeRunnerLog(logPath, QStringLiteral("conpty.input.failed"), QStringLiteral("winError=%1").arg(GetLastError()));
                    return false;
                }
            }
            return !closing.load();
        });
        SetEvent(stopEvent.get());
    });
    const HANDLE waitHandles[] = {childProcess.get(), stopEvent.get()};
    const DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
    DWORD exitCode = 0;
    if (waitResult == WAIT_OBJECT_0) GetExitCodeProcess(childProcess.get(), &exitCode);
    else exitCode = waitResult == WAIT_OBJECT_0 + 1 ? 0 : 5;
    closing.store(true);
    // Session shutdown is scoped to this Runner's job, never other desktop processes.
    TerminateJobObject(job, exitCode);
    CancelSynchronousIo(inputThread.native_handle());
    inputThread.join();
    closeAndDrain(waitResult == WAIT_OBJECT_0);
    inputWrite.reset();
    writeRunnerLog(logPath, QStringLiteral("conpty.finished"), QStringLiteral("exitCode=%1").arg(exitCode));
    return static_cast<int>(exitCode);
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
    const int transportIndex = arguments.indexOf(QStringLiteral("--transport"));
    const bool transportRequested = transportIndex >= 0 || arguments.contains(QStringLiteral("--conpty"));
    const QString transport = arguments.contains(QStringLiteral("--conpty")) ? QStringLiteral("conpty")
        : transportIndex >= 0 && transportIndex + 1 < arguments.size() ? arguments.at(transportIndex + 1) : QStringLiteral("pipe");
    const bool sessionMode = arguments.contains(QStringLiteral("--session"));
    if ((!sessionMode && (commandIndex < 0 || commandIndex + 1 >= arguments.size()))
        || cwdIndex < 0 || cwdIndex + 1 >= arguments.size()
        || shellIndex < 0 || shellIndex + 1 >= arguments.size()
        || (transportIndex >= 0 && transportIndex + 1 >= arguments.size())
        || (transport != QStringLiteral("auto") && transport != QStringLiteral("pipe") && transport != QStringLiteral("conpty")))
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
        | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY
        | JOB_OBJECT_LIMIT_JOB_MEMORY;
    limits.BasicLimitInformation.ActiveProcessLimit = 32;
    limits.ProcessMemoryLimit = 1024ULL * 1024ULL * 1024ULL;
    limits.JobMemoryLimit = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        writeRunnerLog(logPath, QStringLiteral("job.limit.failed"), QStringLiteral("winError=%1").arg(GetLastError()));
        CloseHandle(job);
        return 3;
    }
    else
        writeRunnerLog(logPath, QStringLiteral("job.limit.applied"),
            QStringLiteral("activeProcesses=32 processMemoryMiB=1024 jobMemoryMiB=4096"));

    QProcess shell;
    const QString requestedShell = arguments.at(shellIndex + 1);
    const QString resolvedShell = QStandardPaths::findExecutable(requestedShell);
    const QString shellProgram = resolvedShell.isEmpty() ? requestedShell : resolvedShell;
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
        if (name.contains(QLatin1Char('=')) || name.contains(QChar(u'\0')) || value.contains(QChar(u'\0'))) continue;
        environment.insert(name, value);
    }
    if (sessionMode && transport != QStringLiteral("pipe")) {
        auto dimension = [&](const QString &option, int fallback, int lower, int upper) {
            const int index = arguments.indexOf(option);
            if (index < 0) return fallback;
            bool valid = false;
            const int value = index + 1 < arguments.size() ? arguments.at(index + 1).toInt(&valid) : 0;
            return valid && value >= lower && value <= upper ? value : -1;
        };
        const int columns = dimension(QStringLiteral("--columns"), 80, 20, 400);
        const int rows = dimension(QStringLiteral("--rows"), 24, 5, 200);
        if (columns < 0 || rows < 0) { CloseHandle(job); return 2; }
        if (const auto result = runPseudoConsole(shellProgram, commandPrompt, arguments.at(cwdIndex + 1),
                                                environment, job, logPath, columns, rows)) {
            CloseHandle(job);
            return *result;
        }
        writeRunnerLog(logPath, QStringLiteral("conpty.fallback"), QStringLiteral("requested=%1").arg(transport));
        if (transport == QStringLiteral("conpty")) {
            const QByteArray message("__SIDEBAR_TRANSPORT_ERROR__\n");
            fwrite(message.constData(), 1, static_cast<size_t>(message.size()), stderr);
            fflush(stderr);
            CloseHandle(job);
            return 6;
        }
    }
    if (transportRequested) {
        const QByteArray marker("__SIDEBAR_TRANSPORT_PIPE__\n");
        fwrite(marker.constData(), 1, static_cast<size_t>(marker.size()), stdout);
        fflush(stdout);
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
        std::atomic_bool closing{false};
        std::thread inputThread([&shell, commandPrompt, &closing] {
            readRequests(closing, [&](const Request &request) {
                if (request.kind == Request::Close) return false;
                if (request.kind != Request::Command && request.kind != Request::Control) return !closing.load();
                QMetaObject::invokeMethod(&shell, [request, commandPrompt, &shell] {
                    QString wrapped = wrappedCommand(request, commandPrompt);
                    if (!commandPrompt) wrapped.replace(QLatin1Char('\r'), QLatin1Char('\n'));
                    shell.write(wrapped.toUtf8());
                }, Qt::QueuedConnection);
                return !closing.load();
            });
            QMetaObject::invokeMethod(&shell, [&shell] { shell.closeWriteChannel(); }, Qt::QueuedConnection);
        });
        const int code = app.exec();
        closing.store(true);
        CancelSynchronousIo(inputThread.native_handle());
        inputThread.join();
        return code;
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
