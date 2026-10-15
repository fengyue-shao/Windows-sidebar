#include "include/pluginregistry.h"
#include "include/islandbridge.h"
#include "include/pluginprotocol.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QLocalSocket>
#include <QTimer>
#include <QThread>
#include <QJsonArray>
#include <iostream>
#define CHECK(v) do { if (!(v)) { std::cerr << "FAIL " << __LINE__ << ": " << #v << '\n'; return 1; } } while(false)
static bool waitUntil(const std::function<bool()> &predicate) {
    QElapsedTimer clock; clock.start();
    while (!predicate() && clock.elapsed()<6000) { QCoreApplication::processEvents(QEventLoop::AllEvents,20); QThread::msleep(5); }
    return predicate();
}
static int fakePlugin(QCoreApplication &app) {
    QLocalSocket socket; PluginProtocol::Decoder decoder;
    auto send=[&](const QJsonObject &object){socket.write(QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n'); socket.flush();};
    QObject::connect(&socket,&QLocalSocket::connected,&app,[&]{send({{"type","hello"},{"protocolVersion",1},
        {"pluginId",qEnvironmentVariable("SIDEBAR_PLUGIN_ID")},{"token",qEnvironmentVariable("SIDEBAR_PLUGIN_TOKEN")}});});
    QObject::connect(&socket,&QLocalSocket::readyRead,&app,[&]{for (const auto &object:decoder.feed(socket.readAll())) {
        if (object.value("type")=="ready") send({{"type","result"},{"schemaVersion",1},{"source","result"},{"key","fixture-1"},{"priority",85},
            {"result",QJsonObject{{"pageId","main"},{"label","Fixture"},{"outcome","failed"},{"exitCode",7},{"elapsedMs",1234}}}});
        if (object.value("type")=="close") QTimer::singleShot(250,&app,&QCoreApplication::quit);
    }});
    QTimer::singleShot(10000,&app,&QCoreApplication::quit);
    socket.connectToServer(qEnvironmentVariable("SIDEBAR_PLUGIN_ENDPOINT")); return app.exec();
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); app.setApplicationVersion("0.8.2.0");
    if (qEnvironmentVariableIsSet("SIDEBAR_PLUGIN_ENDPOINT")) return fakePlugin(app);
    app.setOrganizationName("IsolatedTest"); app.setApplicationName("PluginVersions");
    QTemporaryDir temporary; CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path()+"/settings");
    auto package=[&](const QString &version) {
        const QString path=temporary.path()+"/source-"+version; QDir().mkpath(path+"/bin");
        if (!QFile::copy(app.applicationFilePath(),path+"/bin/fixture.exe")) return QString();
        QFile exe(path+"/bin/fixture.exe"); if (!exe.open(QIODevice::ReadOnly)) return QString();
        QCryptographicHash hash(QCryptographicHash::Sha256); if (!hash.addData(&exe)) return QString();
        const QJsonObject manifest{{"schemaVersion",2},{"apiVersion",1},{"runtime","process"},{"id","test.lifecycle"},{"name","Fixture"},
            {"version",version},{"minimumHostVersion","0.8.2.0"},{"entry","bin/fixture.exe"},
            {"files",QJsonObject{{"bin/fixture.exe",QString::fromLatin1(hash.result().toHex())}}},{"pages",QJsonArray{QJsonObject{{"id","main"}}}}};
        QFile file(path+"/plugin.json"); if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(manifest).toJson())<=0) return QString();
        return path;
    };
    const auto first=package("1.0.0"), second=package("2.0.0"); CHECK(!first.isEmpty() && !second.isEmpty());
    IslandBridge island; PluginRegistry registry(nullptr,temporary.path()+"/installed"); registry.setIslandBridge(&island);
    CHECK(registry.importTrustedFolder(first)); CHECK(registry.importTrustedFolder(second));
    auto descriptor=[&]{return registry.plugins().first().toMap();};
    CHECK(descriptor().value("version")=="1.0.0" && !descriptor().value("running").toBool());
    registry.launch("test.lifecycle");
    CHECK(waitUntil([&]{return descriptor().value("processStatus")=="running" && island.activeState().value("exitCode")==7;}));
    CHECK(island.activeState().value("tone")=="danger" && island.activeState().value("kind")=="task");
    CHECK(!registry.selectVersion("test.lifecycle","2.0.0"));
    CHECK(descriptor().value("version")=="1.0.0" && descriptor().value("running").toBool());
    CHECK(registry.selectVersion("test.lifecycle","2.0.0",true));
    CHECK(descriptor().value("version")=="1.0.0" && descriptor().value("pendingVersion")=="2.0.0");
    registry.launch("test.lifecycle"); // Blocked while old process owns the slot.
    CHECK(descriptor().value("version")=="1.0.0");
    CHECK(waitUntil([&]{return descriptor().value("version")=="2.0.0" && !descriptor().value("running").toBool();}));
    CHECK(descriptor().value("processStatus")=="stopped" && descriptor().value("pendingVersion").toString().isEmpty());
    CHECK(island.activeState().isEmpty());
    registry.launch("test.lifecycle");
    CHECK(waitUntil([&]{return descriptor().value("processStatus")=="running" && island.activeState().value("exitCode")==7;}));
    registry.stop("test.lifecycle"); CHECK(waitUntil([&]{return !descriptor().value("running").toBool();}));
    CHECK(registry.selectVersion("test.lifecycle","1.0.0"));
    CHECK(!descriptor().value("running").toBool());
    // Older layout/trust remains readable without relocating user packages.
    const auto legacyRoot=temporary.path()+"/legacy"; QDir().mkpath(legacyRoot+"/test.lifecycle/bin");
    CHECK(QFile::copy(first+"/bin/fixture.exe",legacyRoot+"/test.lifecycle/bin/fixture.exe"));
    CHECK(QFile::copy(first+"/plugin.json",legacyRoot+"/test.lifecycle/plugin.json"));
    QSettings settings; settings.remove("plugins/test.lifecycle");
    QFile manifest(first+"/plugin.json"); CHECK(manifest.open(QIODevice::ReadOnly));
    settings.setValue("plugins/test.lifecycle/trustedDigest",QString::fromLatin1(QCryptographicHash::hash(manifest.readAll(),QCryptographicHash::Sha256).toHex()));
    settings.setValue("plugins/test.lifecycle/enabled",true);
    PluginRegistry legacy(nullptr,legacyRoot); CHECK(legacy.plugins().first().toMap().value("version")=="1.0.0");
    CHECK(legacy.plugins().first().toMap().value("enabled").toBool());
    std::cout << "PLUGIN VERSION PASS: real fixture IPC to island result, confirmation, stop-before-switch, no auto-restart, rollback, persisted selection, legacy adoption; temporary data, no shell/system commands\n";
}
