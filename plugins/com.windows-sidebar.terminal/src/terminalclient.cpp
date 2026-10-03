#include "include/terminalclient.h"
#include <QCoreApplication>
#include <QTimer>
#include <QColor>

QString TerminalClient::localSource(const QString &source) {
    return source=="plugin.terminal.running" ? "running" : source=="plugin.terminal" ? "result" : QString();
}
QJsonObject TerminalClient::presentation(const QVariantMap &state) {
    QJsonObject result{{"pageId","terminal"}};
    for (const auto &key:QStringList{"title","subtitle","detailText","iconName","tone","progress"})
        if (state.contains(key)) result.insert(key,QJsonValue::fromVariant(state.value(key)));
    // Legacy terminal UI uses warning; protocol v1 has no warning tone.
    if (result.value("tone")=="warning") result["tone"]="neutral";
    return result;
}
TerminalClient::TerminalClient(QObject *parent):QObject(parent) {
    const auto endpoint=qEnvironmentVariable("SIDEBAR_PLUGIN_ENDPOINT");
    if (endpoint.isEmpty()) return; // Optional development standalone mode.
    const auto token=qEnvironmentVariable("SIDEBAR_PLUGIN_TOKEN");
    const auto id=qEnvironmentVariable("SIDEBAR_PLUGIN_ID");
    // Shell/Runner children must not inherit the host connection credentials.
    qunsetenv("SIDEBAR_PLUGIN_TOKEN"); qunsetenv("SIDEBAR_PLUGIN_ENDPOINT"); qunsetenv("SIDEBAR_PLUGIN_ID");
    connect(&m_socket,&QLocalSocket::connected,this,[this,token,id]{ send({{"type","hello"},{"protocolVersion",1},{"pluginId",id},{"token",token}}); });
    connect(&m_socket,&QLocalSocket::readyRead,this,[this]{
        for (const auto &object:m_decoder.feed(m_socket.readAll())) {
            if (object.value("protocolVersion")!=1) { QCoreApplication::quit(); return; }
            const auto type=object.value("type").toString();
            if (type=="close") { QCoreApplication::quit(); return; }
            if (type=="show") emit showRequested();
            if (type=="ready" || type=="theme") {
                QVariantMap tokens; const auto received=object.value("tokens").toObject();
                for (const auto &key:QStringList{"surface","border","accent","accentSoft","textPrimary","textMuted","textFaint","warning","danger","dangerSoft","success"}) {
                    const auto color=received.value(key).toString();
                    if (color.size()<=9 && color.startsWith('#') && QColor(color).isValid()) tokens.insert(key,color);
                }
                emit themeTokensReceived(tokens);
            }
            if (type=="ready") { m_ready=true; const auto pending=m_pending; m_pending.clear(); for (const auto &state:pending) send(state); }
        }
        if (m_decoder.failed) QCoreApplication::quit();
    });
    connect(&m_socket,&QLocalSocket::disconnected,this,[]{ QCoreApplication::quit(); });
    connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,this,[this]{
        if (m_ready) { send({{"type","closing"}}); m_socket.waitForBytesWritten(100); }
    });
    QTimer::singleShot(8000,this,[this]{ if (!m_ready) QCoreApplication::quit(); });
    m_socket.setReadBufferSize(2*PluginProtocol::MaxFrameBytes); m_socket.connectToServer(endpoint);
}
void TerminalClient::send(const QJsonObject &object) {
    if (m_socket.state()==QLocalSocket::ConnectedState) { m_socket.write(QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n'); m_socket.flush(); }
}
bool TerminalClient::publishState(const QString &source,const QVariantMap &state,int priority) {
    const auto local=localSource(source); if (local.isEmpty()) return false;
    const QJsonObject object{{"type","state"},{"source",local},{"priority",priority},{"state",presentation(state)}};
    if (m_ready) send(object); else m_pending.insert(local,object); return true;
}
bool TerminalClient::pushEvent(const QString &source,const QString &key,const QVariantMap &state,int priority,int duration,int validFor) {
    const auto local=localSource(source); if (local.isEmpty() || !m_ready) return false;
    send({{"type","event"},{"source",local},{"key",key},{"priority",priority},{"durationMs",duration},{"validForMs",validFor},{"state",presentation(state)}}); return true;
}
void TerminalClient::withdraw(const QString &source) {
    const auto local=localSource(source); if (local.isEmpty()) return;
    m_pending.remove(local); if (m_ready) send({{"type","withdraw"},{"source",local}});
}
