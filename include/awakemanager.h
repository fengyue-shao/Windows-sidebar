#pragma once
#include <QObject>
#include <QTimer>
#include <functional>

class AwakeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool keepDisplay READ keepDisplay NOTIFY changed)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int durationSeconds READ durationSeconds NOTIFY changed)
public:
    // Injectable OS and clock functions allow tests without altering power state.
    explicit AwakeManager(QObject *parent = nullptr,
        std::function<bool(bool, bool)> request = {}, std::function<quint64()> clock = {});
    ~AwakeManager() override;
    bool active() const { return m_active; }
    bool keepDisplay() const { return m_keepDisplay; }
    int remainingSeconds() const { return m_remaining; }
    int durationSeconds() const { return m_duration; }
    QString status() const { return m_status; }
    Q_INVOKABLE void start(int minutes, bool keepDisplay);
    Q_INVOKABLE void stop();
signals:
    void changed();
    void expired();
private slots:
    void tick();
private:
    std::function<bool(bool, bool)> m_request;
    std::function<quint64()> m_clock;
    QTimer m_timer;
    quint64 m_deadline = 0;
    bool m_active = false, m_keepDisplay = false;
    bool m_stopRequested = false, m_expired = false;
    int m_duration = 0;
    void release();
    int m_remaining = 0;
    QString m_status = QStringLiteral("已关闭 · 不改变系统电源计划");
};
