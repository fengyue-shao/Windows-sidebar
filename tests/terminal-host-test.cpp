#include "include/terminalmanager.h"
#include "include/terminalscreen.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QElapsedTimer>
#include <QDebug>
#include <iostream>
#include <functional>
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)
static bool waitFor(const std::function<bool()> &predicate, int milliseconds = 10000)
{
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < milliseconds) QTest::qWait(20);
    return predicate();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 2) return 2;
    QTemporaryDir storage(QString::fromLocal8Bit(argv[1]) + "/terminal-test-XXXXXX");
    CHECK(storage.isValid());
    TerminalManager manager(nullptr, storage.path());
    QSignalSpy outcomes(&manager, &TerminalManager::operationFinished);
    QSignalSpy commands(&manager, &TerminalManager::commandCompleted);
    CHECK(!manager.sessionActive() && !manager.sessionStarting());
    manager.setPreferredTransport("pipe");
    manager.run(QStringLiteral("Write-Output '中文 UTF-8'; Write-Output '__SIDEBAR_DONE_0'"));
    CHECK(waitFor([&] { return !manager.running(); }));
    CHECK(commands.size() == 1 && outcomes.size() == 1);
    CHECK(manager.output().contains(QStringLiteral("中文 UTF-8")) && manager.output().contains("__SIDEBAR_DONE_0"));
    CHECK(outcomes.last()[1] == "success" && manager.sessionActive());
    manager.run("Write-Error 'fixture failure'");
    CHECK(waitFor([&] { return !manager.running(); }));
    CHECK(outcomes.size() == 2 && outcomes.last()[1] == "failed");
    manager.run("cmd.exe /D /C exit 7");
    CHECK(waitFor([&] { return !manager.running(); }));
    CHECK(outcomes.size() == 3 && outcomes.last()[1] == "failed" && outcomes.last()[2].toInt() == 7);
    manager.setCommandTimeoutSeconds(1);
    manager.run("Start-Sleep -Seconds 10");
    CHECK(waitFor([&] { return !manager.sessionActive() && !manager.sessionStarting(); }));
    CHECK(!manager.running() && outcomes.size() == 4 && outcomes.last()[1] == "timeout");
    const int before = outcomes.size(); QTest::qWait(200); CHECK(outcomes.size() == before);
    manager.setCommandTimeoutSeconds(60);
    manager.run("Start-Sleep -Seconds 10");
    CHECK(waitFor([&] { return manager.sessionActive(); }));
    manager.stop();
    CHECK(waitFor([&] { return !manager.sessionActive() && !manager.sessionStarting(); }));
    CHECK(outcomes.size() == before + 1 && outcomes.last()[1] == "cancelled");
    manager.run("Start-Sleep -Seconds 10");
    CHECK(waitFor([&] { return manager.sessionActive(); }));
    manager.resetSession();
    CHECK(waitFor([&] { return !manager.sessionActive() && !manager.sessionStarting(); }));
    CHECK(outcomes.last()[1] == "reset" && manager.currentDirectory() == manager.sandboxPath());
    CHECK(manager.completionStart("Get-Content 'space na") == 12);
    CHECK(manager.completionStart("Get-Content space` na") == 12);
    CHECK(manager.completionSuggestions("Get-ChildItem -Re").join('\n').contains("-Recurse"));
    QDir().mkpath(manager.sandboxPath() + "/space name");
    CHECK(manager.completionSuggestions("Get-Content 'space na").join('\n').contains("'space name/'"));
    CHECK(manager.completionSuggestions("Get-Content ../../").isEmpty());
    if (app.arguments().contains("--pipe-only")) {
        QFile log(manager.logPath()); CHECK(log.open(QIODevice::ReadOnly));
        const QByteArray logged = log.readAll();
        CHECK(!logged.contains("fixture failure") && !logged.contains("Start-Sleep -Seconds"));
        std::cout << "PASS pipe UTF8, failure, timeout, cancel, reset, completions, log privacy\n";
        return 0;
    }
    manager.setPreferredTransport("conpty");
    manager.resizeTerminal(80, 18);
    manager.startSession();
    CHECK(waitFor([&] { return manager.transportInteractive() || (!manager.sessionActive() && !manager.sessionStarting()); }, 12000));
    if (!manager.transportInteractive()) {
        std::cerr << "Native startup failed: " << manager.statusText().toStdString() << '\n';
        std::cerr << manager.output().toStdString() << '\n';
        QFile log(manager.logPath()); if (log.open(QIODevice::ReadOnly)) std::cerr << log.readAll().toStdString();
        return 3;
    }
    manager.sendInput("Write-Output ('NATIVE_' + 'READY')\r");
    auto *screen = qobject_cast<TerminalScreen *>(manager.screen());
    CHECK(waitFor([&] { return screen->plainText().contains("NATIVE_READY"); }, 12000));
    const int nativeBefore = outcomes.size();
    manager.setCommandTimeoutSeconds(1); QTest::qWait(1300);
    CHECK(manager.sessionActive() && outcomes.size() == nativeBefore && !manager.running());
    manager.sendInput(QStringLiteral("Write-Output ('中文_' + '完成')\r"));
    CHECK(waitFor([&] { return screen->plainText().contains(QStringLiteral("中文_完成")); }));
    manager.resizeTerminal(40, 10);
    CHECK(screen->columns() == 40 && screen->rows() == 10);
    manager.sendInput("exit\r");
    CHECK(waitFor([&] { return !manager.sessionActive(); }));
    CHECK(outcomes.size() == nativeBefore + 1 && outcomes.last()[0] == "session" && outcomes.last()[1] == "success");
    manager.setPreferredTransport("pipe"); CHECK(!manager.transportInteractive());
    QFile log(manager.logPath()); CHECK(log.open(QIODevice::ReadOnly));
    const QByteArray logged = log.readAll();
    CHECK(!logged.contains("fixture failure") && !logged.contains("NATIVE_READY") && !logged.contains("Start-Sleep -Seconds"));
    qInfo() << "PASS async startup + UTF8 + nonce framing + failure/timeout/cancel/reset + native input + idle lifetime + completion + privacy";
    return 0;
}
