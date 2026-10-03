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
    explicit HotspotManager(QObject *parent = nullptr, bool monitorSystem = true);
    ~HotspotManager() override;
    bool supported() const { return m_supported; }
    bool busy() const { return m_busy; }
    bool active() const { return m_active; }
    QString status() const { return m_status; }
    QString ssid() const { return m_ssid; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setActive(bool active);
    Q_INVOKABLE void configure(const QString &ssid, const QString &password);
    Q_INVOKABLE void cancel();
    static bool validConfiguration(const QString &ssid, const QString &password);
    void finished(quint64 generation, const QString &message);
signals:
    void changed();
    void operationCompleted();
private:
    friend class NativeAsyncTest;
    bool m_supported = false, m_busy = false, m_active = false;
    QString m_status, m_ssid, m_result;
    winrt::Windows::Networking::NetworkOperators::NetworkOperatorTetheringManager m_manager{nullptr};
    winrt::Windows::Foundation::IAsyncAction m_action{nullptr};
    quint64 m_generation = 0;
    bool m_monitorSystem = true;
    quint64 beginOperation(const QString &message);
    void timeout(quint64 generation);
    winrt::Windows::Networking::NetworkOperators::NetworkOperatorTetheringManager resolveManager(bool allowCached);
};
