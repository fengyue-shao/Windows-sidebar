#pragma once

#include <QObject>
#include <QVariantList>
#include <QHash>
#include <array>
#include <winrt/Windows.Devices.Enumeration.h>

class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager() override;
    QVariantList devices() const { return m_devices; }
    QString statusText() const { return m_statusText; }
    bool scanning() const { return m_scanning; }
    bool busy() const { return m_pairing; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void cancelScan();
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE void pairDevice(const QString &deviceId);
    Q_INVOKABLE void unpairDevice(const QString &deviceId);

    void applyOperation(const QString &status, quint64 generation);

signals:
    void devicesChanged();
    void statusChanged();
    void scanningChanged();
    void busyChanged();

private:
    friend class BluetoothLifecycleTest;
    QVariantList m_devices;
    QString m_statusText = QStringLiteral("打开蓝牙面板后扫描设备");
    bool m_scanning = false;
    bool m_pairing = false;
    quint64 m_scanGeneration = 0, m_pairGeneration = 0;
    struct WatchSlot {
        winrt::Windows::Devices::Enumeration::DeviceWatcher watcher{nullptr};
        winrt::event_token added{}, updated{}, removed{}, enumerated{}, stopped{};
        QHash<QString, winrt::Windows::Devices::Enumeration::DeviceInformation> information;
        QHash<QString, QVariantMap> rows;
        bool complete = false;
    };
    std::array<WatchSlot, 4> m_watchers;
    QHash<QString, QVariantMap> m_previousRows;
    winrt::Windows::Foundation::IAsyncAction m_pairAction{nullptr};
    void beginPairing(const QString &id, bool pair);
    void stopWatchers();
    bool accepts(size_t source, quint64 generation) const;
    void acceptDevice(size_t source, const QVariantMap &row, quint64 generation);
    void removeDevice(size_t source, const QString &id, quint64 generation);
    void enumerationComplete(size_t source, quint64 generation);
    void watcherFailed(quint64 generation, const QString &message);
    void publishDevices();
    void finishScan(quint64 generation);
};
