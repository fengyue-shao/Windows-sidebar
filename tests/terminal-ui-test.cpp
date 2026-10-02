#include "include/terminalscreen.h"
#include "include/terminalcanvas.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QInputMethodEvent>
#include <QPainter>
#include <QtTest/QTest>
#include <QDebug>
#include <iostream>
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)
class MockTerminal : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString output MEMBER output NOTIFY outputChanged)
    Q_PROPERTY(QString statusText MEMBER statusText NOTIFY statusChanged)
    Q_PROPERTY(QString currentDirectory MEMBER directory NOTIFY statusChanged)
    Q_PROPERTY(QString sandboxPath MEMBER directory CONSTANT)
    Q_PROPERTY(QString shellName MEMBER shellName CONSTANT)
    Q_PROPERTY(QString transportName MEMBER transportName NOTIFY transportChanged)
    Q_PROPERTY(QString preferredTransport MEMBER preferredTransport NOTIFY transportChanged)
    Q_PROPERTY(QObject* screen READ screen CONSTANT)
    Q_PROPERTY(bool sessionActive MEMBER active NOTIFY sessionChanged)
    Q_PROPERTY(bool sessionStarting MEMBER starting NOTIFY sessionChanged)
    Q_PROPERTY(bool transportInteractive MEMBER interactive NOTIFY transportChanged)
    Q_PROPERTY(bool running MEMBER running NOTIFY runningChanged)
    Q_PROPERTY(bool conPtyAvailable MEMBER available CONSTANT)
    Q_PROPERTY(int sessionEnvironmentCount MEMBER environmentCount NOTIFY statusChanged)
    Q_PROPERTY(qint64 elapsedMilliseconds MEMBER elapsed NOTIFY elapsedChanged)
    Q_PROPERTY(int commandTimeoutSeconds MEMBER timeout NOTIFY statusChanged)
public:
    TerminalScreen display;
    QString output, statusText = "Ready", directory = "C:/fixture", shellName = "PowerShell", transportName = "ConPTY", preferredTransport = "auto";
    bool active = false, starting = false, interactive = false, running = false, available = true;
    int environmentCount = 0, timeout = 60, starts = 0, resizeCount = 0;
    qint64 elapsed = 0;
    QStringList inputs, commands;
    QObject *screen() { return &display; }
    Q_INVOKABLE void resizeTerminal(int columns, int rows) { ++resizeCount; display.resize(columns, rows); }
    Q_INVOKABLE void startSession() { ++starts; starting = true; active = interactive = false; emit sessionChanged(); }
    void markProcessRunning() { starting = false; active = true; emit sessionChanged(); }
    void completeTransport(bool native) {
        interactive = native; transportName = native ? "ConPTY" : "Pipe";
        emit transportChanged(); emit sessionChanged();
    }
    Q_INVOKABLE void sendInput(const QString &input) { inputs.append(input); }
    Q_INVOKABLE void run(const QString &command) { commands.append(command); }
    Q_INVOKABLE void stop() { active = starting = interactive = running = false; emit sessionChanged(); emit transportChanged(); emit runningChanged(); }
    Q_INVOKABLE void resetSession() { stop(); display.clear(); }
    Q_INVOKABLE void clear() { display.clear(); output.clear(); emit outputChanged(); }
    Q_INVOKABLE void openLogFile() {}
    Q_INVOKABLE int completionStart(const QString &input) const { return input.lastIndexOf(' ') + 1; }
    Q_INVOKABLE QStringList completionSuggestions(const QString &input) const { return input == "Get-Ch" ? QStringList{"Get-ChildItem\tcommand\tfixture"} : QStringList{}; }
signals:
    void outputChanged(); void statusChanged(); void transportChanged(); void sessionChanged(); void runningChanged(); void elapsedChanged();
    void operationFinished(const QString &, const QString &, int, qint64);
};
class MockIsland : public QObject {
    Q_OBJECT
public:
    QVariantMap state;
    QList<QVariantMap> events;
    int statePriority = 0;
    Q_INVOKABLE void publishState(const QString &, const QVariantMap &value, int priority) { state = value; statePriority = priority; }
    Q_INVOKABLE void withdraw(const QString &) { state.clear(); }
    Q_INVOKABLE bool pushEvent(const QString &, const QString &, const QVariantMap &value, int priority, int, int) {
        auto recorded = value; recorded.insert("priority", priority); events.append(recorded); return true;
    }
};
int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    qmlRegisterType<TerminalCanvas>("WindowsSidebar.Terminal", 1, 0, "TerminalCanvas");
    if (argc != 3) return 2;
    MockTerminal manager;
    MockIsland island;
    TerminalScreen cells;
    cells.resize(20, 4); cells.feed(QStringLiteral("\x1b[41m中\x1b[42mB").toUtf8());
    TerminalCanvas canvas; canvas.setScreen(&cells); canvas.setWidth(40); canvas.setHeight(16);
    canvas.setCellWidth(7); canvas.setCellHeight(16);
    QImage grid(40, 16, QImage::Format_ARGB32_Premultiplied); grid.fill(Qt::transparent);
    { QPainter painter(&grid); canvas.paint(&painter); }
    CHECK(grid.pixelColor(1, 1) == QColor("#ef6b73") && grid.pixelColor(15, 1) == QColor("#85c88a"));
    QQmlPropertyMap theme;
    for (const auto &name : {"textPrimary", "textMuted", "textFaint", "textSecondary", "accent", "warning", "danger", "success", "surface", "border", "accentSoft", "dangerSoft"})
        theme.insert(name, QString(name) == "surface" ? "#17202B" : QString(name) == "border" ? "#304052" : QString(name).endsWith("Soft") ? "#203D59" : "#DDE8F4");
    QQuickView view;
    view.setColor(QColor("#101720"));
    view.rootContext()->setContextProperty("terminalManager", &manager);
    view.rootContext()->setContextProperty("islandBridge", &island);
    view.rootContext()->setContextProperty("themeManager", &theme);
    int warnings = 0;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &errors) {
        warnings += errors.size(); for (const auto &error : errors) std::cerr << error.toString().toStdString() << '\n';
    });
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(480, 860);
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    CHECK(view.rootObject());
    view.show(); view.requestActivate(); QTest::qWait(300);
    CHECK(manager.starts == 0 && manager.resizeCount > 0);
    auto *raw = view.rootObject()->findChild<QQuickItem *>("nativeTerminalInput");
    auto *draft = view.rootObject()->findChild<QQuickItem *>("terminalCommandInput");
    auto *preview = view.rootObject()->findChild<QQuickItem *>("terminalCompletionPreview");
    CHECK(raw && draft && preview);
    // Match QProcess: Starting -> Running, then an asynchronous transport header.
    // The old synchronous mock hid the invisible-draft focus regression.
    CHECK(QMetaObject::invokeMethod(view.rootObject(), "startTerminalSession"));
    CHECK(manager.starting && !manager.active && raw->hasActiveFocus());
    manager.markProcessRunning(); QTest::qWait(30);
    CHECK(manager.active && !manager.interactive && !manager.starting);
    CHECK(view.rootObject()->property("nativeMode").toBool() && raw->isVisible() && !draft->isVisible());
    CHECK(raw->hasActiveFocus() && raw->property("readOnly").toBool());
    QTest::keyClick(&view, 'A'); QTest::keyClick(&view, 'R');
    CHECK(manager.inputs.isEmpty() && raw->property("text").toString().isEmpty() && draft->property("text").toString().isEmpty());
    manager.completeTransport(true); QTest::qWait(50);
    CHECK(raw->hasActiveFocus() && !raw->property("readOnly").toBool() && !draft->isVisible());
    QTest::keyClick(&view, 'A'); QTest::keyClick(&view, 'R');
    if (manager.inputs != QStringList({"A", "R"})) qWarning() << "Fixture A/R input received:" << manager.inputs;
    CHECK(manager.inputs == QStringList({"A", "R"}));
    CHECK(raw->property("text").toString().isEmpty() && draft->property("text").toString().isEmpty());
    CHECK(island.statePriority == 10 && island.state.value("title") == QStringLiteral("交互终端已打开"));
    emit manager.operationFinished("command", "timeout", -1, 1250);
    CHECK(island.events.size() == 1 && island.events.last().value("priority") == 90 && island.events.last().value("tone") == "warning");
    emit manager.operationFinished("command", "cancelled", -1, 2500);
    CHECK(island.events.size() == 2 && island.events.last().value("priority") == 80);
    emit manager.operationFinished("session", "reset", 0, 1000);
    CHECK(island.events.size() == 2);
    QTest::keyClick(&view, Qt::Key_Tab); QTest::qWait(30);
    CHECK(raw->hasActiveFocus() && manager.inputs.last() == "\t");
    QTest::keyClick(&view, Qt::Key_Backtab); QTest::qWait(30);
    CHECK(raw->hasActiveFocus() && manager.inputs.last() == "\x1b[Z");
    QTest::keyClick(&view, Qt::Key_Left); QTest::keyClick(&view, Qt::Key_C, Qt::ControlModifier);
    CHECK(manager.inputs[manager.inputs.size() - 2] == "\x1b[D" && manager.inputs.last() == QString(QChar(3)));
    QInputMethodEvent commit; commit.setCommitString(QStringLiteral("中文"));
    QCoreApplication::sendEvent(raw, &commit); QTest::qWait(40);
    CHECK(manager.inputs.last() == QStringLiteral("中文"));
    manager.display.feed("\x1b[?2004h");
    const auto beforePaste = manager.inputs.size();
    CHECK(QMetaObject::invokeMethod(view.rootObject(), "sendPaste", Q_ARG(QVariant, QVariant("fixture paste"))));
    CHECK(manager.inputs.size() == beforePaste + 1 && manager.inputs.last() == "\x1b[200~fixture paste\x1b[201~");
    view.rootObject()->setProperty("assistantVisible", true); QTest::qWait(50); draft->forceActiveFocus();
    draft->setProperty("text", "Get-Ch"); QTest::qWait(200);
    CHECK(preview->isVisible() && preview->property("text").toString().contains("Get-ChildItem"));
    QTest::keyClick(&view, Qt::Key_Tab); QTest::qWait(30);
    CHECK(draft->hasActiveFocus() && draft->property("text") == "Get-ChildItem");
    const int beforeDraft = manager.inputs.size(); QTest::keyClick(&view, Qt::Key_Return); QTest::qWait(30);
    CHECK(manager.inputs.size() == beforeDraft + 1 && manager.inputs.last() == "\x1b[200~Get-ChildItem\x1b[201~");
    CHECK(!manager.inputs.last().contains('\r') && !manager.inputs.last().contains('\n'));
    CHECK(!island.state.value("subtitle").toString().contains("Get-ChildItem"));
    view.rootObject()->setProperty("assistantVisible", false);
    manager.display.feed(QStringLiteral("\r\n\x1b[31m红色\x1b[0m UTF-8\r\n").toUtf8());
    QTest::qWait(100);
    view.rootObject()->setProperty("copyMode", true); QTest::qWait(40);
    auto *selection = view.rootObject()->findChild<QQuickItem *>("terminalOutputSelection");
    CHECK(selection && selection->property("text").toString().contains("UTF-8"));
    manager.display.feed("NEW_COPY_OUTPUT"); QTest::qWait(40);
    CHECK(selection->property("text").toString().contains("NEW_COPY_OUTPUT"));
    view.rootObject()->setProperty("copyMode", false);
    CHECK(!view.grabWindow().isNull());
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2]) + "/terminal-native-preview.png"));

    // A user choosing another surface while starting owns focus, even after the header arrives.
    manager.stop();
    CHECK(QMetaObject::invokeMethod(view.rootObject(), "startTerminalSession"));
    manager.markProcessRunning();
    view.rootObject()->setProperty("assistantVisible", true); QTest::qWait(30);
    draft->forceActiveFocus(); draft->setProperty("text", "user draft");
    manager.completeTransport(true); QTest::qWait(50);
    CHECK(view.rootObject()->property("assistantVisible").toBool() && draft->hasActiveFocus());
    CHECK(draft->property("text") == "user draft");
    emit manager.transportChanged(); QTest::qWait(30);
    CHECK(draft->hasActiveFocus());
    manager.stop(); view.rootObject()->setProperty("assistantVisible", false); draft->setProperty("text", "");
    CHECK(QMetaObject::invokeMethod(view.rootObject(), "startTerminalSession"));
    manager.markProcessRunning();
    view.rootObject()->setProperty("copyMode", true); QTest::qWait(30); selection->forceActiveFocus();
    manager.completeTransport(true); QTest::qWait(50);
    CHECK(view.rootObject()->property("copyMode").toBool() && selection->hasActiveFocus() && !raw->hasActiveFocus());

    // Automatic fallback must move focus to the visible compatibility editor, not a hidden raw field.
    manager.stop();
    CHECK(QMetaObject::invokeMethod(view.rootObject(), "startTerminalSession"));
    manager.markProcessRunning(); manager.completeTransport(false); QTest::qWait(50);
    CHECK(!view.rootObject()->property("nativeMode").toBool() && draft->isVisible() && draft->hasActiveFocus());
    manager.stop(); manager.preferredTransport = "pipe"; emit manager.transportChanged();
    QTest::qWait(100); draft->forceActiveFocus(); draft->setProperty("text", "echo fixture");
    QTest::keyClick(&view, Qt::Key_Return); CHECK(manager.commands.last() == "echo fixture");
    for (const QSize size : {QSize(360, 780), QSize(960, 560)}) {
        view.resize(size); QTest::qWait(200); CHECK(!view.grabWindow().isNull());
        CHECK(view.rootObject()->width() == size.width() && view.rootObject()->height() == size.height());
    }
    auto *confirmation = view.rootObject()->findChild<QObject *>("terminalPasteConfirmation");
    CHECK(confirmation);
    confirmation->setProperty("visible", true); QTest::qWait(100);
    CHECK(confirmation->property("height").toReal() > 112);
    confirmation->setProperty("visible", false);
    CHECK(warnings == 0);
    std::cout << "PASS cell grid + no auto-start + async startup focus + A/R character input + user focus ownership + pipe fallback + native Tab/focus + Ctrl+C/arrows + IME commit + bracketed paste + assistant preview + insert-only + pipe mode + layouts + island outcomes\n";
    return 0;
}
#include "terminal-ui-test.moc"
