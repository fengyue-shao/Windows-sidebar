#pragma once

#include <QObject>
#include <QVariantList>

class WifiManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)

public:
    explicit WifiManager(QObject *parent = nullptr);

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

    QVariantList m_networks;
    QString m_statusText = QStringLiteral("正在读取 Wi-Fi 网络");
    bool m_scanning = false;
};
