#include "include/terminalmanager.h"
#include "include/terminalcanvas.h"
#include "include/terminalclient.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QTimer>
#include <QWindow>
#include <QQuickWindow>

int main(int argc,char **argv) {
    qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    QGuiApplication app(argc,argv);
    app.setOrganizationName("WindowsSidebar"); app.setApplicationName("TerminalPlugin"); app.setApplicationVersion("0.8.2.0");
    qmlRegisterType<TerminalCanvas>("WindowsSidebar.Terminal",1,0,"TerminalCanvas");
    TerminalClient bridge;
    TerminalManager terminal;
    QQmlPropertyMap theme;
    const QVariantMap colors{{"surface","#171B23"},{"border","#303948"},{"accent","#72A7FF"},{"accentSoft","#253B58"},
        {"textPrimary","#EDF2FA"},{"textMuted","#A9B5C6"},{"textFaint","#7D899C"},{"warning","#E6B766"},
        {"danger","#F07885"},{"dangerSoft","#4A2731"},{"success","#83D6AC"}};
    for (auto it=colors.begin();it!=colors.end();++it) theme.insert(it.key(),it.value());
    QObject::connect(&bridge,&TerminalClient::themeTokensReceived,&theme,[&](const QVariantMap &tokens){
        for (auto it=tokens.begin();it!=tokens.end();++it) theme.insert(it.key(),it.value());
    });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("terminalManager",&terminal);
    engine.rootContext()->setContextProperty("islandBridge",&bridge);
    engine.rootContext()->setContextProperty("themeManager",&theme);
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&terminal,&TerminalManager::stop);
    engine.load(QUrl("qrc:/ui/Window.qml"));
    if (engine.rootObjects().isEmpty()) return 2;
    QObject::connect(&bridge,&TerminalClient::showRequested,&engine,[&]{
        auto *window=qobject_cast<QWindow *>(engine.rootObjects().first());
        if (window) { window->showNormal(); window->raise(); window->requestActivate(); }
    });
    if (app.arguments().contains("--ui-smoke")) QTimer::singleShot(1500,&app,[&]{
        for (const auto &argument:app.arguments()) if (argument.startsWith("--ui-capture=")) {
            auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            if (!window || !window->grabWindow().save(argument.mid(13))) { app.exit(3); return; }
        }
        app.quit();
    });
    return app.exec();
}
