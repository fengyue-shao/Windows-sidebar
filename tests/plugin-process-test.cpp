#include "include/pluginprocess.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <iostream>
#define CHECK(v) do { if (!(v)) { std::cerr << "FAIL " << __LINE__ << '\n'; return 1; } } while(false)
static bool waitUntil(const std::function<bool()> &predicate,int timeout=6000) {
    QElapsedTimer elapsed; elapsed.start();
    while (!predicate() && elapsed.elapsed()<timeout) { QCoreApplication::processEvents(QEventLoop::AllEvents,20); QThread::msleep(5); }
    return predicate();
}
static int child(QCoreApplication &app) {
    QLocalSocket socket; PluginProtocol::Decoder decoder;
    auto write=[&](const QJsonObject &object) { socket.write(QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n'); socket.flush(); };
    QObject::connect(&socket,&QLocalSocket::connected,&app,[&]{
        write({{"type","hello"},{"protocolVersion",1},{"pluginId",qEnvironmentVariable("SIDEBAR_PLUGIN_ID")},
            {"token",qEnvironmentVariableIsSet("SIDEBAR_TEST_BAD_TOKEN") ? "wrong" : qEnvironmentVariable("SIDEBAR_PLUGIN_TOKEN")}});
    });
    QObject::connect(&socket,&QLocalSocket::readyRead,&app,[&]{
        for (const auto &frame:decoder.feed(socket.readAll())) {
            const auto type=frame.value("type").toString();
            if (type=="close") { app.quit(); return; }
            if (type=="ready" || type=="show") {
                write({{"type","state"},{"source","running"},{"priority",10},
                    {"state",QJsonObject{{"title",type},{"pageId","main"}}}});
                if (type=="ready" && qEnvironmentVariableIsSet("SIDEBAR_TEST_EXIT")) QTimer::singleShot(100,&app,[&]{ app.exit(7); });
            }
        }
    });
    QObject::connect(&socket,&QLocalSocket::disconnected,&app,&QCoreApplication::quit);
    QTimer::singleShot(10000,&app,[&]{ app.exit(8); });
    socket.connectToServer(qEnvironmentVariable("SIDEBAR_PLUGIN_ENDPOINT")); return app.exec();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if (qEnvironmentVariableIsSet("SIDEBAR_PLUGIN_ENDPOINT")) return child(app);
    PluginProcess process("test.plugin",{"main"});
    int messages=0,withdrawals=0; QString lastTitle,source;
    QObject::connect(&process,&PluginProcess::messageReceived,&app,[&](const QString &type,const QString &id,const QVariantMap &state,int priority,const QString &,int,int){
        if (type=="state" && priority==10) { ++messages; lastTitle=state.value("title").toString(); source=id; }
    });
    QObject::connect(&process,&PluginProcess::sourcesWithdrawn,&app,[&](const QStringList &ids){ if (ids.contains(source)) ++withdrawals; });
    CHECK(!process.start("Z:/definitely-missing-sidebar-plugin.exe") && process.status()=="start-failed");
    CHECK(process.start(QCoreApplication::applicationFilePath()));
    CHECK(waitUntil([&]{ return process.status()=="running" && messages==1; }));
    CHECK(lastTitle=="ready" && source==PluginProtocol::namespaceFor("test.plugin")+".running");
    process.show(); CHECK(waitUntil([&]{ return messages==2; }) && lastTitle=="show");
    process.stop(); CHECK(waitUntil([&]{ return !process.running(); }));
    CHECK(process.status()=="stopped" && withdrawals>0);
    qputenv("SIDEBAR_TEST_BAD_TOKEN","1");
    CHECK(process.start(QCoreApplication::applicationFilePath()));
    CHECK(waitUntil([&]{ return process.status()=="protocol-error" && !process.running(); }));
    qunsetenv("SIDEBAR_TEST_BAD_TOKEN");
    CHECK(messages==2);
    qputenv("SIDEBAR_TEST_EXIT","1");
    CHECK(process.start(QCoreApplication::applicationFilePath()));
    CHECK(waitUntil([&]{ return !process.running() && (process.status()=="crashed" || process.status()=="disconnected"); }));
    qunsetenv("SIDEBAR_TEST_EXIT");
    CHECK(messages==3 && withdrawals>1);
    CHECK(process.start(QCoreApplication::applicationFilePath()));
    CHECK(waitUntil([&]{ return process.status()=="running" && messages==4; }));
    process.stop(); CHECK(waitUntil([&]{ return !process.running(); }));
    std::cout << "PLUGIN PROCESS PASS: actual child handshake/state/show/stop, bad token rejected, unexpected exit withdraws sources, restart; no terminal/system commands\n";
}
