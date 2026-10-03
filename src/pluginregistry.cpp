#include "include/pluginregistry.h"
#include "include/pluginprocess.h"
#include "include/islandbridge.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QUuid>
#include <QVersionNumber>
#include <QDirIterator>

namespace {
QString rootPath() { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/trusted-plugins"; }
struct Package { QString id,name,version,digest,entry,permissions; QSet<QString> pages; QStringList files; };
bool loadPackage(const QString &path,Package &package,QString &error) {
    const QDir directory(QFileInfo(path).absoluteFilePath()); QFile manifestFile(directory.filePath("plugin.json"));
    auto reject=[&](const QString &why){ error=why; return false; };
    if (QFileInfo(path).isSymLink() || QFileInfo(manifestFile.fileName()).isSymLink() || !manifestFile.open(QIODevice::ReadOnly) || manifestFile.size()>256*1024) return reject("缺少或过大的清单");
    const auto bytes=manifestFile.readAll(); QJsonParseError parse;
    const auto document=QJsonDocument::fromJson(bytes,&parse); const auto manifest=document.object();
    if (parse.error!=QJsonParseError::NoError || !document.isObject() || manifest.value("schemaVersion")!=2
        || manifest.value("apiVersion")!=1 || manifest.value("runtime")!="process") return reject("只接受独立进程 schema 2 / API 1 插件；旧共享 QML 不加载");
    package.id=manifest.value("id").toString(); package.name=manifest.value("name").toString(); package.version=manifest.value("version").toString();
    if (!QRegularExpression("^[a-z0-9][a-z0-9.-]{2,63}$").match(package.id).hasMatch() || package.name.isEmpty() || package.name.size()>128
        || QVersionNumber::fromString(package.version).isNull()) return reject("ID、名称或版本无效");
    const auto minimum=QVersionNumber::fromString(manifest.value("minimumHostVersion").toString());
    if (minimum.isNull() || QVersionNumber::compare(QVersionNumber::fromString(QCoreApplication::applicationVersion()),minimum)<0) return reject("宿主版本不满足要求");
    for (const auto &value:manifest.value("pages").toArray()) {
        const auto id=value.toObject().value("id").toString();
        if (!PluginProtocol::localId(id) || package.pages.contains(id) || package.pages.size()>=8) return reject("页面 ID 无效或重复");
        package.pages.insert(id);
    }
    if (package.pages.isEmpty()) return reject("没有可打开的插件页面");
    const auto files=manifest.value("files").toObject();
    if (files.isEmpty() || files.size()>4096) return reject("缺少有界完整性清单");
    qint64 total=0;
    for (auto it=files.begin();it!=files.end();++it) {
        const auto relative=it.key();
        if (relative.isEmpty() || relative=="plugin.json" || relative.contains('\\') || relative.contains(':') || relative.startsWith('/')
            || relative.split('/').contains("..") || relative.split('/').contains(".") || relative.contains("//")) return reject("文件路径越界");
        const QFileInfo info(directory.filePath(relative));
        const auto canonical=info.canonicalFilePath(); const auto within=directory.relativeFilePath(canonical);
        if (!info.isFile() || info.isSymLink() || canonical.isEmpty() || QDir::isAbsolutePath(within) || within.startsWith("../")) return reject("文件缺失或越界");
        auto parent=info.dir();
        while (parent.absolutePath()!=directory.absolutePath()) {
            if (QFileInfo(parent.absolutePath()).isSymLink() || parent.isRoot()) return reject("文件父路径不可使用链接");
            parent.cdUp();
        }
        total+=info.size(); if (info.size()>256*1024*1024 || total>512*1024*1024) return reject("插件包过大");
        QFile file(info.absoluteFilePath()); QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file) || QString::fromLatin1(hash.result().toHex())!=it.value().toString()) return reject("文件完整性不匹配："+relative);
        package.files.append(relative);
    }
    const auto entry=manifest.value("entry").toString();
    if (!files.contains(entry) || !entry.endsWith(".exe",Qt::CaseInsensitive)) return reject("独立进程入口无效");
    package.entry=QFileInfo(directory.filePath(entry)).canonicalFilePath();
    QDirIterator entries(directory.absolutePath(),QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDirIterator::Subdirectories);
    int entryCount=0;
    while (entries.hasNext()) {
        entries.next(); if (++entryCount>8192 || entries.fileInfo().isSymLink()) return reject("额外路径或链接超出限制");
        if (entries.fileInfo().isFile()) { const auto relative=directory.relativeFilePath(entries.filePath());
            if (relative!="plugin.json" && !files.contains(relative)) return reject("发现未列入完整性清单的文件："+relative);
        }
    }
    package.digest=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    QStringList permissions; for (const auto &permission:manifest.value("permissions").toArray()) if (permission.isString()) permissions.append(permission.toString().left(96));
    package.permissions=permissions.join(" · "); return true;
}
}
QString PluginRegistry::packageError(const QString &path) { Package package; QString error; loadPackage(path,package,error); return error; }
PluginRegistry::PluginRegistry(QObject *parent,const QString &pluginRoot):QObject(parent),m_rootPath(pluginRoot.isEmpty() ? rootPath() : pluginRoot) { reload(); }
PluginRegistry::~PluginRegistry() { qDeleteAll(m_processes); m_processes.clear(); }
void PluginRegistry::setIslandBridge(IslandBridge *bridge) { m_bridge=bridge; }
void PluginRegistry::setThemeTokens(const QVariantMap &tokens) { m_themeTokens=tokens; for (auto *process:m_processes) process->setThemeTokens(tokens); }
void PluginRegistry::reload() {
    QVariantList discovered; QStringList errors; QSet<QString> valid;
    m_executables.clear(); m_routes.clear(); m_pageIds.clear();
    const QDir root(m_rootPath);
    for (const auto &info:root.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)) {
        Package package; QString error;
        if (!loadPackage(info.absoluteFilePath(),package,error)) { errors.append(info.fileName()+": "+error); continue; }
        if (valid.contains(package.id)) { errors.append(package.id+": ID 重复"); continue; }
        valid.insert(package.id); QSettings settings;
        const bool trusted=settings.value("plugins/"+package.id+"/trustedDigest").toString()==package.digest;
        const bool enabled=trusted && settings.value("plugins/"+package.id+"/enabled",false).toBool();
        discovered.append(QVariantMap{{"id",package.id},{"name",package.name},{"version",package.version},{"trusted",trusted},
            {"digest",package.digest},{"enabled",enabled},{"pageCount",package.pages.size()},{"permissionsText",package.permissions}});
        if (!enabled) continue;
        m_executables.insert(package.id,package.entry); m_pageIds.insert(package.id,package.pages);
        for (const auto &page:package.pages) m_routes.insert(PluginProtocol::namespaceFor(package.id)+'.'+page,package.id);
    }
    for (auto it=m_processes.begin();it!=m_processes.end();++it) if (!m_executables.contains(it.key())) it.value()->stop();
    m_plugins=discovered; m_pages.clear(); // No foreign QML/nav pages; toolbox owns launch/management.
    m_errorDetails=errors.join('\n'); m_statusText=QString("发现 %1 个独立插件，%2 个错误").arg(m_plugins.size()).arg(errors.size());
    updateProcessStatus(); emit pagesChanged(); emit statusChanged();
}
void PluginRegistry::updateProcessStatus() {
    for (auto &item:m_plugins) { auto descriptor=item.toMap(); const auto process=m_processes.value(descriptor.value("id").toString());
        descriptor.insert("processStatus",process ? process->status() : "stopped"); item=descriptor; }
    emit pluginsChanged();
}
void PluginRegistry::setPluginEnabled(const QString &id,bool enabled) {
    for (const auto &item:m_plugins) { const auto descriptor=item.toMap();
        if (descriptor.value("id").toString()!=id || (enabled && !descriptor.value("trusted").toBool())) continue;
        QSettings().setValue("plugins/"+id+"/enabled",enabled); if (!enabled) stop(id); reload(); return;
    }
}
void PluginRegistry::trustAndEnable(const QString &id) {
    for (const auto &item:m_plugins) { const auto descriptor=item.toMap(); if (descriptor.value("id").toString()!=id) continue;
        QSettings settings; settings.setValue("plugins/"+id+"/trustedDigest",descriptor.value("digest")); settings.setValue("plugins/"+id+"/enabled",true); reload(); return;
    }
}
void PluginRegistry::launch(const QString &id) {
    reload(); if (!m_executables.contains(id)) return; // Check hashes/trust again at execution.
    auto *process=m_processes.value(id);
    if (process && !process->running()) { m_processes.remove(id); process->deleteLater(); process=nullptr; }
    if (!process) {
        process=new PluginProcess(id,m_pageIds.value(id),this); m_processes.insert(id,process);
        process->setThemeTokens(m_themeTokens);
        connect(process,&PluginProcess::statusChanged,this,&PluginRegistry::updateProcessStatus);
        connect(process,&PluginProcess::sourcesWithdrawn,this,[this](const QStringList &sources){ if (m_bridge) for (const auto &source:sources) m_bridge->withdraw(source); });
        connect(process,&PluginProcess::messageReceived,this,[this](const QString &type,const QString &source,const QVariantMap &state,int priority,const QString &key,int duration,int validFor){
            if (!m_bridge) return;
            if (type=="state") m_bridge->publishState(source,state,priority);
            else if (type=="event") m_bridge->pushEvent(source,key,state,priority,duration,validFor);
            else if (type=="withdraw") m_bridge->withdraw(source);
        });
    }
    if (process->running()) process->show(); else process->start(m_executables.value(id)); updateProcessStatus();
}
void PluginRegistry::stop(const QString &id) { if (auto *process=m_processes.value(id)) process->stop(); }
bool PluginRegistry::openRoute(const QString &route) { const auto id=m_routes.value(route); if (id.isEmpty()) return false; launch(id); return true; }
void PluginRegistry::openPluginFolder() { QDir().mkpath(m_rootPath); QDesktopServices::openUrl(QUrl::fromLocalFile(m_rootPath)); }
bool PluginRegistry::importTrustedFolder(const QString &path) {
    const QUrl url(path); const QString source=QFileInfo(url.isLocalFile() ? url.toLocalFile() : path).absoluteFilePath();
    Package package; QString error;
    if (!loadPackage(source,package,error)) { m_errorDetails=error; emit statusChanged(); return false; }
    const auto destination=m_rootPath+'/'+package.id;
    if (QFileInfo::exists(destination)) { m_errorDetails="该 ID 已安装；首版不覆盖，请备份并移出旧包"; emit statusChanged(); return false; }
    const QString temporary=m_rootPath+"/.import-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(temporary); auto files=package.files; files.append("plugin.json");
    for (const auto &file:files) {
        const auto target=temporary+'/'+file; QDir().mkpath(QFileInfo(target).absolutePath());
        if (!QFile::copy(source+'/'+file,target)) { QDir(temporary).removeRecursively(); m_errorDetails="复制插件失败，未安装"; emit statusChanged(); return false; }
    }
    Package copied;
    if (!loadPackage(temporary,copied,error) || copied.digest!=package.digest || !QDir().rename(temporary,destination)) {
        QDir(temporary).removeRecursively(); m_errorDetails="复制后校验或安装失败："+error; emit statusChanged(); return false;
    }
    QSettings settings; settings.setValue("plugins/"+package.id+"/trustedDigest",package.digest); settings.setValue("plugins/"+package.id+"/enabled",true);
    reload(); return true;
}
