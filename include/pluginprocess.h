#pragma once
#include "pluginprotocol.h"
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>

// One trusted plugin per instance. No plugin-specific classes or UI live here.
class PluginProcess : public QObject {
    Q_OBJECT
public:
    PluginProcess(QString pluginId,QSet<QString> pages,QObject *parent=nullptr);
    ~PluginProcess() override;
    bool start(const QString &executable);
    void show();
    void stop();
    void setThemeTokens(const QVariantMap &tokens);
    QString status() const { return m_status; }
    bool running() const { return m_process.state()!=QProcess::NotRunning; }
signals:
    void statusChanged();
    void stopped();
    void messageReceived(const QString &type,const QString &sourceId,const QVariantMap &state,int priority,const QString &key,int durationMs,int validForMs);
    void sourcesWithdrawn(const QStringList &sourceIds);
private:
    QString m_id,m_status="stopped";
    QSet<QString> m_pages;
    QProcess m_process;
    QLocalServer m_server;
    QPointer<QLocalSocket> m_socket;
    QTimer m_handshakeTimer,m_killTimer;
    QElapsedTimer m_clock;
    PluginProtocol::Decoder m_decoder;
    std::unique_ptr<PluginProtocol::Session> m_session;
    bool m_stopping=false;
    QVariantMap m_themeTokens;
    void setStatus(const QString &value);
    void acceptConnection();
    void readMessages();
    void send(const QString &type);
    void fail(const QString &status);
    void withdraw();
    void cleanup();
};
