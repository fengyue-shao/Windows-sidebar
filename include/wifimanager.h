#pragma once

#include <QObject>
#include <QVariantList>
#include <QSet>
#include <Windows.h>
#include <wlanapi.h>
#include "wificonnectionstate.h"

class WifiManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QVariantList adapters READ adapters NOTIFY networksChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)

public:
    explicit WifiManager(QObject *parent = nullptr);
    ~WifiManager() override;

    QVariantList networks() const { return m_networks; }
    QVariantList adapters() const { return m_adapters; }
    QString statusText() const { return m_statusText; }
    bool scanning() const { return m_scanning; }
    bool connecting() const { return m_connection.active; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void connectNetwork(const QString &ssid, const QString &profileName,
                                    const QString &password, bool secure, const QByteArray &interfaceId = {});
    Q_INVOKABLE void connectHiddenNetwork(const QString &ssid, const QString &password, bool secure, const QByteArray &interfaceId);
    Q_INVOKABLE void disconnectNetwork(const QString &ssid, const QString &profile, const QByteArray &interfaceId);
    Q_INVOKABLE void forgetNetwork(const QString &ssid, const QString &profile, const QByteArray &interfaceId, bool confirmDisconnect);

signals:
    void networksChanged();
    void statusChanged();
    void scanningChanged();
    void connectingChanged();

private:
    void loadNetworks();
    void setStatus(const QString &text);
    static void WINAPI scanNotification(PWLAN_NOTIFICATION_DATA data, PVOID context);
    void closeScan();
    void scanFinished(quint64 generation, const QByteArray &adapter, bool failed);
    static void WINAPI connectionNotification(PWLAN_NOTIFICATION_DATA data, PVOID context);
    void connectionEvent(quint64 generation, const QByteArray &adapter, const QByteArray &ssid, const QString &profile, DWORD code, DWORD reason);
    void closeConnection();
    bool connectionConfirmed() const;
    void finishConnection(const QString &status);
    void requestDisconnect(const QString &ssid, const QString &profile, const QByteArray &adapter, bool forget);
    bool disconnectedConfirmed() const;
    void finishDisconnection();
    struct ScanContext { WifiManager *owner = nullptr; quint64 generation = 0; };
    ScanContext m_scanContext;
    ScanContext m_connectionContext;
    HANDLE m_connectionHandle = nullptr;
    SidebarWifi::ConnectionState m_connection;
    bool m_disconnecting = false;
    QString m_forgetAfterDisconnect;
    HANDLE m_scanHandle = nullptr;
    QSet<QByteArray> m_pendingAdapters;
    quint64 m_scanGeneration = 0;
    bool m_scanFailed = false;

    QVariantList m_networks;
    QVariantList m_adapters;
    QString m_statusText = QStringLiteral("正在读取 Wi-Fi 网络");
    bool m_scanning = false;
};
