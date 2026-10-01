#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <memory>

class AudioInputManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY inputChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY inputChanged)
    Q_PROPERTY(int volume READ volume NOTIFY inputChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY inputChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY inputChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY inputChanged)
    Q_PROPERTY(double inputPeak READ inputPeak NOTIFY levelsChanged)
    Q_PROPERTY(double outputPeak READ outputPeak NOTIFY levelsChanged)
    Q_PROPERTY(bool inputMeterAvailable READ inputMeterAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool outputMeterAvailable READ outputMeterAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool meteringEnabled READ meteringEnabled WRITE setMeteringEnabled NOTIFY meteringEnabledChanged)
public:
    explicit AudioInputManager(QObject *parent = nullptr);
    ~AudioInputManager() override;
    bool available() const;
    QString deviceName() const;
    int volume() const;
    bool muted() const;
    QString statusText() const;
    QVariantList devices() const;
    double inputPeak() const;
    double outputPeak() const;
    bool inputMeterAvailable() const;
    bool outputMeterAvailable() const;
    bool meteringEnabled() const;
    void setMeteringEnabled(bool enabled);
    Q_INVOKABLE void setVolume(int value);
    Q_INVOKABLE void toggleMuted();
    Q_INVOKABLE void openInputSettings();
    Q_INVOKABLE void refresh();
signals:
    void inputChanged();
    void levelsChanged();
    void meteringEnabledChanged();
private:
    struct State;
    std::unique_ptr<State> d;
    QTimer m_refreshTimer, m_meterTimer;
    void pollLevels();
};
