#pragma once

#include <QObject>
#include <QVariantList>

class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    QVariantList devices() const { return m_devices; }
    QString statusText() const { return m_statusText; }
    bool scanning() const { return m_scanning; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void pairDevice(const QString &deviceId);
    Q_INVOKABLE void unpairDevice(const QString &deviceId);

    void applyDevices(const QVariantList &devices, const QString &status);
    void applyOperation(const QString &status);

signals:
    void devicesChanged();
    void statusChanged();
    void scanningChanged();

private:
    QVariantList m_devices;
    QString m_statusText = QStringLiteral("打开蓝牙面板后扫描设备");
    bool m_scanning = false;
};
