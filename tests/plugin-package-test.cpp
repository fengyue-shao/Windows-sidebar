#include "include/pluginregistry.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <iostream>
#define CHECK(v) do { if (!(v)) { std::cerr << "FAIL " << __LINE__ << '\n'; return 1; } } while(false)
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); app.setApplicationVersion("0.8.2.0");
    app.setOrganizationName("IsolatedTest"); app.setApplicationName("PluginPackage");
    if (app.arguments().size()==2) {
        const auto error=PluginRegistry::packageError(app.arguments().at(1));
        if (!error.isEmpty()) { std::cerr << error.toStdString() << '\n'; return 1; }
        std::cout << "REAL PLUGIN PACKAGE VALIDATION PASS: deployed files, hashes, paths, API and version; no install/execute\n"; return 0;
    }
    QTemporaryDir temporary; CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path()+"/settings");
    const auto source=temporary.path()+"/source"; QDir().mkpath(source+"/bin");
    QFile executable(source+"/bin/test.exe"); CHECK(executable.open(QIODevice::WriteOnly)); executable.write("not an executable; never launched"); executable.close();
    const auto hash=QString::fromLatin1(QCryptographicHash::hash("not an executable; never launched",QCryptographicHash::Sha256).toHex());
    QJsonObject manifest{{"schemaVersion",2},{"apiVersion",1},{"runtime","process"},{"id","test.plugin"},{"name","Test"},
        {"version","1.0.0"},{"minimumHostVersion","0.8.2.0"},{"entry","bin/test.exe"},
        {"files",QJsonObject{{"bin/test.exe",hash}}},{"pages",QJsonArray{QJsonObject{{"id","main"}}}}};
    auto writeManifest=[&](const QJsonObject &value) { QFile file(source+"/plugin.json"); if (!file.open(QIODevice::WriteOnly)) return false; return file.write(QJsonDocument(value).toJson())>0; };
    CHECK(writeManifest(manifest)); CHECK(PluginRegistry::packageError(source).isEmpty());
    CHECK(PluginRegistry::packageError(QDir::current().relativeFilePath(source)).isEmpty());
    auto bad=manifest; bad["schemaVersion"]=1; CHECK(writeManifest(bad)); CHECK(!PluginRegistry::packageError(source).isEmpty());
    bad=manifest; bad["minimumHostVersion"]="99.0.0"; CHECK(writeManifest(bad)); CHECK(!PluginRegistry::packageError(source).isEmpty());
    bad=manifest; bad["version"]="1.0.0/../../other"; CHECK(writeManifest(bad)); CHECK(!PluginRegistry::packageError(source).isEmpty());
    bad=manifest; bad["files"]=QJsonObject{{"../outside.exe",hash}}; CHECK(writeManifest(bad)); CHECK(!PluginRegistry::packageError(source).isEmpty());
    bad=manifest; bad["files"]=QJsonObject{{"bin/test.exe","wrong"}}; CHECK(writeManifest(bad)); CHECK(!PluginRegistry::packageError(source).isEmpty());
    CHECK(writeManifest(manifest));
    QFile extra(source+"/bin/extra.dll"); CHECK(extra.open(QIODevice::WriteOnly)); extra.write("extra"); extra.close();
    CHECK(!PluginRegistry::packageError(source).isEmpty()); CHECK(extra.remove());
    PluginRegistry registry(nullptr,temporary.path()+"/installed");
    CHECK(registry.plugins().isEmpty() && registry.pages().isEmpty());
    CHECK(registry.importTrustedFolder(source));
    CHECK(registry.plugins().size()==1 && registry.pages().isEmpty() && registry.pageCount()==0);
    CHECK(registry.plugins().first().toMap().value("enabled").toBool());
    CHECK(registry.plugins().first().toMap().value("processStatus")=="stopped"); // Never auto-execute.
    CHECK(!registry.importTrustedFolder(source)); // Never overwrite an existing installation.
    manifest["version"]="1.0.0.0"; CHECK(writeManifest(manifest)); CHECK(!registry.importTrustedFolder(source));
    manifest["version"]="2.0.0"; CHECK(writeManifest(manifest)); CHECK(registry.importTrustedFolder(source));
    CHECK(registry.plugins().size()==1 && registry.plugins().first().toMap().value("versions").toList().size()==2);
    CHECK(registry.plugins().first().toMap().value("version")=="1.0.0"); // Import is not a switch.
    CHECK(registry.selectVersion("test.plugin","2.0.0"));
    CHECK(registry.plugins().first().toMap().value("version")=="2.0.0");
    CHECK(registry.plugins().first().toMap().value("processStatus")=="stopped");
    CHECK(!registry.selectVersion("test.plugin","99.0.0"));
    CHECK(registry.selectVersion("test.plugin","1.0.0"));
    { PluginRegistry reopened(nullptr,temporary.path()+"/installed"); CHECK(reopened.plugins().first().toMap().value("version")=="1.0.0"); }
    registry.setPluginEnabled("test.plugin",false); CHECK(!registry.plugins().first().toMap().value("enabled").toBool());
    registry.setPluginEnabled("test.plugin",true); CHECK(registry.plugins().first().toMap().value("enabled").toBool());
    QFile installed(temporary.path()+"/installed/test.plugin@1.0.0/bin/test.exe"); CHECK(installed.open(QIODevice::Append)); installed.write("tampered"); installed.close();
    registry.reload(); CHECK(registry.plugins().size()==1 && !registry.errorDetails().isEmpty());
    CHECK(!registry.plugins().first().toMap().value("enabled").toBool()); // Do not silently use 2.0.0.
    CHECK(registry.plugins().first().toMap().value("version")=="1.0.0");
    CHECK(registry.selectVersion("test.plugin","2.0.0"));
    CHECK(registry.plugins().first().toMap().value("enabled").toBool());
    const auto duplicate=temporary.path()+"/installed/duplicate"; QDir().mkpath(duplicate+"/bin");
    CHECK(QFile::copy(source+"/plugin.json",duplicate+"/plugin.json"));
    CHECK(QFile::copy(source+"/bin/test.exe",duplicate+"/bin/test.exe"));
    registry.reload(); CHECK(!registry.errorDetails().isEmpty() && registry.plugins().isEmpty());
    std::cout << "PLUGIN PACKAGE PASS: API/version/path/hash/extra-file rejection, explicit import, no auto-run, no overwrite, enable/disable, tamper rejection; temporary settings/data only\n";
}
