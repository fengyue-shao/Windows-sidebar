#pragma once
#include "include/pluginprotocol.h"
#include <QObject>
#include <QLocalSocket>
#include <QHash>

// Plugin-owned bridge proxy: no references to host services or QML objects.
class TerminalClient : public QObject {
    Q_OBJECT
public:
    explicit TerminalClient(QObject *parent=nullptr);
    Q_INVOKABLE bool publishState(const QString &source,const QVariantMap &state,int priority);
    Q_INVOKABLE bool pushEvent(const QString &source,const QString &key,const QVariantMap &state,int priority,int duration=4000,int validFor=15000);
    Q_INVOKABLE void withdraw(const QString &source);
signals:
    void showRequested();
    void themeTokensReceived(const QVariantMap &tokens);
private:
    QLocalSocket m_socket;
    PluginProtocol::Decoder m_decoder;
    QHash<QString,QJsonObject> m_pending;
    bool m_ready=false;
    void send(const QJsonObject &object);
    static QString localSource(const QString &source);
    static QJsonObject presentation(const QVariantMap &state);
};
