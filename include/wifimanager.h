#pragma once

#include <QObject>
#include <QVariantList>
#include <QSet>
#include <Windows.h>
#include <wlanapi.h>

class WifiManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)

public:
    explicit WifiManager(QObject *parent = nullptr);
    ~WifiManager() override;

    QVariantList networks() const { return m_networks; }
    QString statusText() const { return m_statusText; }
    bool scanning() const { return m_scanning; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void connectNetwork(const QString &ssid, const QString &profileName,
                                    const QString &password, bool secure);

signals:
    void networksChanged();
    void statusChanged();
    void scanningChanged();

private:
    void loadNetworks();
    void setStatus(const QString &text);
    static void WINAPI scanNotification(PWLAN_NOTIFICATION_DATA data, PVOID context);
    void closeScan();
    void scanFinished(quint64 generation, const QByteArray &adapter, bool failed);
    struct ScanContext { WifiManager *owner = nullptr; quint64 generation = 0; };
    ScanContext m_scanContext;
    HANDLE m_scanHandle = nullptr;
    QSet<QByteArray> m_pendingAdapters;
    quint64 m_scanGeneration = 0;
    bool m_scanFailed = false;

    QVariantList m_networks;
    QString m_statusText = QStringLiteral("正在读取 Wi-Fi 网络");
    bool m_scanning = false;
};
