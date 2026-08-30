#pragma once
#include <QObject>
class RadioManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY stateChanged)
public:
    explicit RadioManager(QObject *parent = nullptr);
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool bluetoothEnabled() const { return m_bluetoothEnabled; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setWifiEnabled(bool enabled);
    Q_INVOKABLE void setBluetoothEnabled(bool enabled);
    void applyState(bool wifi, bool bluetooth);
signals:
    void stateChanged();
private:
    bool m_wifiEnabled = false;
    bool m_bluetoothEnabled = false;
};
