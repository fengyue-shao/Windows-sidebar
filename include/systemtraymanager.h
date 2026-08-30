#pragma once

#include <QObject>
#include <QTimer>

struct IAudioEndpointVolume;

class SystemTrayManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY audioChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY audioChanged)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY powerChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY powerChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY powerChanged)

public:
    explicit SystemTrayManager(QObject *parent = nullptr);
    ~SystemTrayManager() override;

    int volume() const;
    bool muted() const;
    bool batteryPresent() const;
    int batteryPercent() const;
    bool charging() const;

    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void toggleMuted();

signals:
    void audioChanged();
    void powerChanged();

private slots:
    void refresh();

private:
    bool ensureAudioEndpoint();
    void refreshAudio();
    void refreshPower();

    QTimer m_timer;
    IAudioEndpointVolume *m_audioEndpoint = nullptr;
    int m_volume = 0;
    int m_batteryPercent = 0;
    bool m_muted = false;
    bool m_batteryPresent = false;
    bool m_charging = false;
};
