#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

struct IAudioEndpointVolume;
struct IMMDevice;

class SystemTrayManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY audioChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY audioChanged)
    Q_PROPERTY(QString audioDeviceName READ audioDeviceName NOTIFY audioChanged)
    Q_PROPERTY(QVariantList audioOutputs READ audioOutputs NOTIFY audioOutputsChanged)
    Q_PROPERTY(QString audioOutputStatus READ audioOutputStatus NOTIFY audioOutputStatusChanged)
    Q_PROPERTY(bool audioOutputSwitchFailed READ audioOutputSwitchFailed NOTIFY audioOutputStatusChanged)
    Q_PROPERTY(QVariantList audioSessions READ audioSessions NOTIFY audioSessionsChanged)
    Q_PROPERTY(bool online READ online NOTIFY networkChanged)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY powerChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY powerChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY powerChanged)

public:
    explicit SystemTrayManager(QObject *parent = nullptr);
    ~SystemTrayManager() override;

    int volume() const;
    bool muted() const;
    QString audioDeviceName() const;
    QVariantList audioSessions() const;
    QVariantList audioOutputs() const;
    QString audioOutputStatus() const { return m_audioOutputStatus; }
    bool audioOutputSwitchFailed() const { return m_audioOutputSwitchFailed; }
    bool online() const;
    bool batteryPresent() const;
    int batteryPercent() const;
    bool charging() const;

    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void toggleMuted();
    Q_INVOKABLE void setSessionVolume(const QString &sessionId, int volume);
    Q_INVOKABLE void toggleSessionMuted(const QString &sessionId);
    Q_INVOKABLE void selectAudioOutput(const QString &deviceId);

signals:
    void audioChanged();
    void audioSessionsChanged();
    void audioOutputsChanged();
    void audioOutputStatusChanged();
    void powerChanged();
    void networkChanged();

private slots:
    void refresh();

private:
    bool ensureAudioEndpoint();
    void refreshAudio();
    void refreshAudioSessions();
    void refreshAudioOutputs();
    void resetAudioEndpoint();
    void setAudioOutputStatus(bool failed, const QString &status);
    bool updateSession(const QString &sessionId, int volume, bool toggleMute);
    void refreshPower();
    void refreshNetwork();

    QTimer m_timer;
    IAudioEndpointVolume *m_audioEndpoint = nullptr;
    IMMDevice *m_audioDevice = nullptr;
    QVariantList m_audioSessions;
    QVariantList m_audioOutputs;
    int m_volume = 0;
    int m_batteryPercent = 0;
    QString m_audioDeviceName;
    QString m_audioOutputStatus;
    bool m_audioOutputSwitchFailed = false;
    bool m_muted = false;
    bool m_batteryPresent = false;
    bool m_charging = false;
    bool m_online = false;
};
