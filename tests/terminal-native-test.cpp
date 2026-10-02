#include "include/terminalmanager.h"
#include "include/terminalscreen.h"
#include "include/terminalcanvas.h"
#include "include/islandbridge.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <Windows.h>
#include <functional>
#include <iostream>

// Unlike the UI fixture, this test uses the production manager, Runner and an
// actual CMD child attached to ConPTY. Only this process's default-shell profile
// and data directory are isolated; no user setting or execution policy changes.
class ScopedEnvironment final
{
public:
    ScopedEnvironment(const QByteArray &name, const QByteArray &value)
        : m_name(name), m_previous(qgetenv(name.constData())), m_present(qEnvironmentVariableIsSet(name.constData()))
    { qputenv(m_name.constData(), value); }
    ~ScopedEnvironment()
    {
        if (m_present) qputenv(m_name.constData(), m_previous);
        else qunsetenv(m_name.constData());
    }
private:
    QByteArray m_name, m_previous;
    bool m_present;
};

static bool waitFor(const std::function<bool()> &predicate, int milliseconds = 8000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < milliseconds) QTest::qWait(20);
    return predicate();
}

static QByteArray readLog(const TerminalManager &manager)
{
    QFile file(manager.logPath());
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

static bool require(bool condition, const char *description, const TerminalManager &manager)
{
    if (condition) return true;
    std::cerr << "FAIL " << description << '\n'
              << "Status: " << manager.statusText().toStdString() << '\n'
              << "Display:\n" << qobject_cast<TerminalScreen *>(manager.screen())->plainText().toStdString() << '\n'
              << "Isolated log:\n" << readLog(manager).toStdString();
    return false;
}

#define CHECK_NATIVE(condition) do { if (!require((condition), #condition, manager)) return 1; } while (false)

static QString lastDisplayLine(const TerminalScreen &screen)
{
    const QStringList lines = screen.plainText().trimmed().split(QLatin1Char('\n'));
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

static bool isolatedRunnersExited(const QByteArray &log)
{
    const QRegularExpression expression(QStringLiteral("\\[runner:(\\d+)\\]"));
    auto matches = expression.globalMatch(QString::fromUtf8(log));
    QSet<DWORD> checked;
    while (matches.hasNext()) {
        const DWORD pid = matches.next().captured(1).toULong();
        if (checked.contains(pid)) continue;
        checked.insert(pid);
        const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) {
            if (GetLastError() == ERROR_INVALID_PARAMETER) continue; // Already reaped.
            return false;
        }
        const DWORD state = WaitForSingleObject(process, 0);
        CloseHandle(process);
        if (state != WAIT_OBJECT_0) return false;
    }
    return !checked.isEmpty();
}

static int interactivePreview(QGuiApplication &app, TerminalManager &manager, const QString &pluginPath)
{
    qmlRegisterType<TerminalCanvas>("WindowsSidebar.Terminal", 1, 0, "TerminalCanvas");
    IslandBridge island;
    QQmlPropertyMap theme;
    for (const auto *name : {"textPrimary", "textMuted", "textFaint", "textSecondary", "accent", "warning", "danger", "success", "surface", "border", "accentSoft", "dangerSoft"})
        theme.insert(name, QString(name) == "surface" ? "#17202B"
            : QString(name) == "border" ? "#304052"
            : QString(name).endsWith("Soft") ? "#203D59" : "#DDE8F4");
    QQuickView view;
    view.setTitle(QStringLiteral("Native terminal - isolated CMD input test (closes after 5 minutes)"));
    view.setColor(QColor("#101720"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(760, 860);
    view.rootContext()->setContextProperty("terminalManager", &manager);
    view.rootContext()->setContextProperty("islandBridge", &island);
    view.rootContext()->setContextProperty("themeManager", &theme);
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [](const QList<QQmlError> &errors) {
        for (const auto &error : errors) std::cerr << error.toString().toStdString() << '\n';
    });
    view.setSource(QUrl::fromLocalFile(QFileInfo(pluginPath).absoluteFilePath()));
    CHECK_NATIVE(view.rootObject() && view.status() != QQuickView::Error);
    view.show();
    view.requestActivate();
    // Use the production Start action, which records pending native focus before
    // the asynchronous transport handshake instead of focusing a hidden editor.
    QTimer::singleShot(0, &app, [&] {
        QMetaObject::invokeMethod(view.rootObject(), "startTerminalSession");
    });
    QTimer::singleShot(5 * 60 * 1000, &view, &QQuickView::close);
    std::cout << "Isolated real CMD preview. Type A/R, Backspace, Enter or echo commands.\n"
              << "No user Terminal settings or PowerShell trust decisions are involved.\n"
              << "Temporary log: " << manager.logPath().toStdString() << '\n';
    const int result = app.exec();
    manager.stop();
    CHECK_NATIVE(waitFor([&] { return !manager.sessionActive() && !manager.sessionStarting(); }, 3000));
    return result;
}

int main(int argc, char **argv)
{
    bool preview = false;
    for (int index = 1; index < argc; ++index)
        if (QByteArray(argv[index]) == "--interactive-preview") preview = true;
    if (!preview) qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int previewIndex = arguments.indexOf(QStringLiteral("--interactive-preview"));
    if (arguments.size() < 2 || (preview && previewIndex + 1 >= arguments.size())) {
        std::cerr << "Usage: sidebar-terminal-native-test <storage-parent> [--interactive-preview <plugin-Main.qml>]\n";
        return 2;
    }
    QTemporaryDir storage(QDir(arguments.at(1)).absoluteFilePath(QStringLiteral("terminal-native-test-XXXXXX")));
    if (!storage.isValid()) return 2;
    const QString localAppData = storage.path() + QStringLiteral("/local-app-data");
    const QString settingsPath = localAppData + QStringLiteral("/Microsoft/Windows Terminal/settings.json");
    if (!QDir().mkpath(QFileInfo(settingsPath).absolutePath())) return 2;
    QFile settings(settingsPath);
    if (!settings.open(QIODevice::WriteOnly)) return 2;
    const QString profile = QStringLiteral("{b2e56c88-e29c-4bc3-8885-d5e1efa72d0d}");
    const QJsonObject root{
        {QStringLiteral("defaultProfile"), profile},
        {QStringLiteral("profiles"), QJsonObject{{QStringLiteral("list"), QJsonArray{QJsonObject{
            {QStringLiteral("guid"), profile},
            {QStringLiteral("name"), QStringLiteral("Isolated CMD fixture")},
            {QStringLiteral("commandline"), QStringLiteral("\"%SystemRoot%\\System32\\cmd.exe\"")}
        }}}}}
    };
    const QByteArray settingsBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (settings.write(settingsBytes) != settingsBytes.size()) return 2;
    settings.close();
    ScopedEnvironment isolatedLocalAppData("LOCALAPPDATA", localAppData.toUtf8());
    TerminalManager manager(nullptr, storage.path());
    CHECK_NATIVE(manager.shellName().contains(QStringLiteral("cmd.exe"), Qt::CaseInsensitive));
    CHECK_NATIVE(manager.conPtyAvailable());
    manager.setPreferredTransport(QStringLiteral("conpty"));
    manager.resizeTerminal(80, 24);
    if (preview) return interactivePreview(app, manager, arguments.at(previewIndex + 1));

    auto *screen = qobject_cast<TerminalScreen *>(manager.screen());
    CHECK_NATIVE(screen);
    QSignalSpy outcomes(&manager, &TerminalManager::operationFinished);
    manager.startSession();
    CHECK_NATIVE(waitFor([&] { return manager.transportInteractive(); }, 10000));
    CHECK_NATIVE(waitFor([&] { return lastDisplayLine(*screen).endsWith(QLatin1Char('>')); }));
    QTest::qWait(100);

    // Match the failing user case: individual raw characters, not a framed
    // command or a bulk paste. The protocol prefix and base64 must never echo.
    for (const QString &letter : {QStringLiteral("A"), QStringLiteral("R")}) {
        manager.sendInput(letter);
        CHECK_NATIVE(waitFor([&] { return lastDisplayLine(*screen).endsWith(QStringLiteral(">") + letter); }));
        CHECK_NATIVE(!screen->plainText().contains(QStringLiteral("QQ==")) && !screen->plainText().contains(QStringLiteral("Ug==")));
        CHECK_NATIVE(!screen->plainText().contains(QStringLiteral("I\t")));
        manager.sendInput(QString(QChar(0x7f))); // The plugin's native Backspace sequence.
        CHECK_NATIVE(waitFor([&] { return lastDisplayLine(*screen).endsWith(QLatin1Char('>')); }));
    }

    const QString marker = QStringLiteral("__NATIVE_EXEC_2f603__");
    manager.sendInput(QStringLiteral("echo ") + marker + QLatin1Char('\r'));
    CHECK_NATIVE(waitFor([&] { return screen->plainText().count(marker) >= 2; }));
    CHECK_NATIVE(waitFor([&] { return lastDisplayLine(*screen).endsWith(QLatin1Char('>')); }));
    const QString unicodeMarker = QStringLiteral("__NATIVE_中文_完成__");
    manager.sendInput(QStringLiteral("echo ") + unicodeMarker + QLatin1Char('\r'));
    CHECK_NATIVE(waitFor([&] { return screen->plainText().count(unicodeMarker) >= 2; }));
    CHECK_NATIVE(!screen->plainText().contains(QChar(0xfffd)));
    CHECK_NATIVE(waitFor([&] { return lastDisplayLine(*screen).endsWith(QLatin1Char('>')); }));

    // Both the repeated handshake size and an actual resize must leave the
    // request reader responsive. Do not clear the local model behind the shell.
    manager.resizeTerminal(80, 24);
    manager.resizeTerminal(100, 30);
    CHECK_NATIVE(screen->columns() == 100 && screen->rows() == 30);
    manager.sendInput(QStringLiteral("echo __NATIVE_AFTER_RESIZE__\r"));
    CHECK_NATIVE(waitFor([&] { return screen->plainText().count(QStringLiteral("__NATIVE_AFTER_RESIZE__")) >= 2; }));
    CHECK_NATIVE(waitFor([&] { return lastDisplayLine(*screen).endsWith(QLatin1Char('>')); }));

    // stop() sends the real X frame. Require the Runner's normal exit, not the
    // host's 800 ms emergency kill, and inspect only this fixture's process IDs.
    QElapsedTimer stopTimer;
    stopTimer.start();
    manager.stop();
    CHECK_NATIVE(waitFor([&] { return !manager.sessionActive() && !manager.sessionStarting(); }, 3000));
    CHECK_NATIVE(stopTimer.elapsed() < 2500);
    CHECK_NATIVE(outcomes.size() == 1 && outcomes.last()[0] == "session"
        && outcomes.last()[1] == "cancelled" && outcomes.last()[2].toInt() == 0);
    const QByteArray logged = readLog(manager);
    CHECK_NATIVE(logged.contains("conpty.input") && logged.contains("conpty.finished exitCode=0"));
    CHECK_NATIVE(logged.contains("session.finished exitCode=0 exitStatus=0"));
    CHECK_NATIVE(!logged.contains("runner.error") && !logged.contains("conpty.input.failed") && !logged.contains("conpty.resize.failed"));
    CHECK_NATIVE(!logged.contains(marker.toUtf8()) && !logged.contains(unicodeMarker.toUtf8())
        && !logged.contains("QQ==") && !logged.contains("Ug=="));
    CHECK_NATIVE(isolatedRunnersExited(logged));
    std::cout << "PASS real CMD ConPTY + individual A/R echo + Backspace + protocol isolation + UTF-8 + same/changed resize + normal X teardown + private logs\n";
    return 0;
}
