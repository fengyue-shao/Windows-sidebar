#pragma once
#include <QObject>
#include <winrt/Windows.Networking.NetworkOperators.h>

class HotspotManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString ssid READ ssid NOTIFY changed)
public:
    explicit HotspotManager(QObject *parent = nullptr);
    bool supported() const { return m_supported; }
    bool busy() const { return m_busy; }
    bool active() const { return m_active; }
    QString status() const { return m_status; }
    QString ssid() const { return m_ssid; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setActive(bool active);
    Q_INVOKABLE void configure(const QString &ssid, const QString &password);
    static bool validConfiguration(const QString &ssid, const QString &password);
    void finished(const QString &message);
signals:
    void changed();
    void operationCompleted();
private:
    bool m_supported = false, m_busy = false, m_active = false;
    QString m_status, m_ssid, m_result;
    winrt::Windows::Networking::NetworkOperators::NetworkOperatorTetheringManager m_manager{nullptr};
    winrt::Windows::Networking::NetworkOperators::NetworkOperatorTetheringManager resolveManager(bool allowCached);
};
