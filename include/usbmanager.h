#pragma once
#include <QObject>
#include <QVariantList>

class UsbManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit UsbManager(QObject *parent = nullptr);
    QVariantList devices() const { return m_devices; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void eject(const QString &id);
    static bool isStorageFunction(const QString &id, const QString &service, bool removable);
    static bool safeDisk(bool usb, bool numberKnown, bool systemDisk, bool bootOrUnknown);
    void finish(const QVariantList &devices, const QString &status);
signals:
    void changed();
private:
    QVariantList m_devices;
    bool m_busy = false;
    QString m_status = QStringLiteral("正在读取 USB 存储设备…");
};
