#pragma once
#include <QObject>
class RadioManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
public:
    explicit RadioManager(QObject *parent = nullptr);
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool bluetoothEnabled() const { return m_bluetoothEnabled; }
    bool busy() const { return m_busy; }
    QString statusText() const { return m_status; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setWifiEnabled(bool enabled);
    Q_INVOKABLE void setBluetoothEnabled(bool enabled);
    void applyState(bool wifi, bool bluetooth);
    void complete(bool valid, bool wifi, bool bluetooth, const QString &status);
signals:
    void stateChanged();
private:
    bool m_wifiEnabled = false;
    bool m_bluetoothEnabled = false;
    bool m_busy = false;
    QString m_status;
    int m_pendingWifi = -1, m_pendingBluetooth = -1;
    void dispatch(int kind, bool enabled);
    void drain();
};
