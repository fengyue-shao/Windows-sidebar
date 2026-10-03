#include "include/pluginprocess.h"
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QUuid>

PluginProcess::PluginProcess(QString id,QSet<QString> pages,QObject *parent)
    :QObject(parent),m_id(std::move(id)),m_pages(std::move(pages)) {
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(1);
    m_handshakeTimer.setSingleShot(true); m_handshakeTimer.setInterval(8000);
    m_killTimer.setSingleShot(true); m_killTimer.setInterval(2000);
    connect(&m_server,&QLocalServer::newConnection,this,&PluginProcess::acceptConnection);
    connect(&m_handshakeTimer,&QTimer::timeout,this,[this]{ fail("handshake-timeout"); });
    connect(&m_killTimer,&QTimer::timeout,this,[this]{ if (running()) m_process.kill(); });
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if (error==QProcess::FailedToStart) { cleanup(); setStatus("start-failed"); }
    });
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exitStatus){
        const auto previous=m_status; const bool stopping=m_stopping;
        cleanup();
        if (previous.endsWith("-failed") || previous=="handshake-timeout" || previous=="protocol-error" || previous=="disconnected") setStatus(previous);
        else setStatus(stopping ? "stopped" : exitStatus==QProcess::CrashExit || code!=0 ? "crashed" : "stopped");
    });
    // Do not forward plugin stdout/stderr into host logs (may contain commands).
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{ m_process.readAllStandardOutput(); });
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{ m_process.readAllStandardError(); });
}
PluginProcess::~PluginProcess() {
    m_handshakeTimer.stop(); m_killTimer.stop();
    disconnect(&m_process,nullptr,this,nullptr);
    if (m_socket) disconnect(m_socket,nullptr,this,nullptr);
    send("close");
    if (running()) { m_process.terminate(); if (!m_process.waitForFinished(500)) { m_process.kill(); m_process.waitForFinished(1000); } }
    cleanup();
}
void PluginProcess::setStatus(const QString &value) { if (m_status!=value) { m_status=value; emit statusChanged(); } }
bool PluginProcess::start(const QString &executable) {
    if (running()) { show(); return false; }
    cleanup();
    const QFileInfo info(executable);
    if (!info.isFile() || info.isSymLink() || !info.isExecutable()) { setStatus("start-failed"); return false; }
    const QString endpoint="WindowsSidebar-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    QByteArray random;
    for (int i=0;i<8;++i) { const quint32 value=QRandomGenerator::system()->generate(); random.append(reinterpret_cast<const char *>(&value),sizeof(value)); }
    const QByteArray token=random.toHex();
    m_session=std::make_unique<PluginProtocol::Session>(m_id,token,m_pages);
    if (!m_server.listen(endpoint)) { cleanup(); setStatus("listen-failed"); return false; }
    auto environment=QProcessEnvironment::systemEnvironment();
    // Preserve Windows user/profile variables. Only add our own scoped protocol values.
    environment.insert("SIDEBAR_PLUGIN_ENDPOINT",endpoint);
    environment.insert("SIDEBAR_PLUGIN_TOKEN",QString::fromLatin1(token));
    environment.insert("SIDEBAR_PLUGIN_ID",m_id);
    m_process.setProcessEnvironment(environment);
    m_process.setProgram(info.canonicalFilePath()); m_process.setArguments({});
    m_process.setWorkingDirectory(info.absolutePath());
    m_stopping=false; m_decoder={}; m_clock.start();
    setStatus("starting"); m_handshakeTimer.start(); m_process.start(); return true;
}
void PluginProcess::acceptConnection() {
    while (m_server.hasPendingConnections()) {
        auto *socket=m_server.nextPendingConnection();
        if (m_socket || m_stopping || !m_session) { socket->abort(); socket->deleteLater(); continue; }
        m_socket=socket; socket->setParent(this); socket->setReadBufferSize(2*PluginProtocol::MaxFrameBytes);
        connect(socket,&QLocalSocket::readyRead,this,&PluginProcess::readMessages);
        connect(socket,&QLocalSocket::disconnected,this,[this]{ if (!m_stopping && running()) fail("disconnected"); });
        readMessages();
    }
}
void PluginProcess::readMessages() {
    if (!m_socket || !m_session || m_stopping) return;
    while (m_socket && m_socket->bytesAvailable()>0 && !m_stopping) {
        const auto frames=m_decoder.feed(m_socket->read(16*1024));
        if (m_decoder.failed) { fail("protocol-error"); return; }
        for (const auto &frame:frames) {
            PluginProtocol::Message message;
            if (!m_session->accept(frame,m_clock.elapsed(),message)) { fail("protocol-error"); return; }
            if (message.type=="hello") { m_handshakeTimer.stop(); setStatus("running"); send("ready"); }
            else emit messageReceived(message.type,message.sourceId,message.state,message.priority,message.key,message.durationMs,message.validForMs);
            if (m_stopping) return;
        }
    }
}
void PluginProcess::send(const QString &type) {
    if (m_socket && m_socket->state()==QLocalSocket::ConnectedState && m_session && m_session->authenticated()) {
        m_socket->write(QJsonDocument(QJsonObject{{"type",type},{"protocolVersion",PluginProtocol::Version}}).toJson(QJsonDocument::Compact)+'\n');
        m_socket->flush();
    }
}
void PluginProcess::show() { if (!m_stopping) send("show"); }
void PluginProcess::withdraw() { if (m_session) emit sourcesWithdrawn(m_session->sourceIds()); }
void PluginProcess::stop() {
    if (m_stopping) return;
    m_stopping=true; m_handshakeTimer.stop(); withdraw(); send("close"); m_server.close();
    if (running()) { setStatus("stopping"); m_killTimer.start(); } else { cleanup(); setStatus("stopped"); }
}
void PluginProcess::fail(const QString &status) { stop(); setStatus(status); }
void PluginProcess::cleanup() {
    m_stopping=true; m_handshakeTimer.stop(); m_killTimer.stop(); m_server.close(); withdraw();
    if (m_socket) { disconnect(m_socket,nullptr,this,nullptr); m_socket->abort(); m_socket->deleteLater(); m_socket=nullptr; }
    m_session.reset(); m_decoder={};
}
