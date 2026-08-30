#pragma once

#include <QObject>
#include <QTimer>

class FocusTimer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY remainingSecondsChanged)
    Q_PROPERTY(int durationSeconds READ durationSeconds NOTIFY durationChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int completedSessions READ completedSessions NOTIFY completedSessionsChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY remainingSecondsChanged)

public:
    explicit FocusTimer(QObject *parent = nullptr);

    int remainingSeconds() const;
    int durationSeconds() const;
    bool running() const;
    int completedSessions() const;
    qreal progress() const;

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void setDurationMinutes(int minutes);
    Q_INVOKABLE void clearCompletedSessions();

signals:
    void remainingSecondsChanged();
    void durationChanged();
    void runningChanged();
    void completedSessionsChanged();
    void sessionCompleted();

private slots:
    void tick();

private:
    void setRunning(bool running);

    QTimer m_timer;
    qint64 m_deadlineMs = 0;
    int m_remainingSeconds = 25 * 60;
    int m_durationSeconds = 25 * 60;
    int m_completedSessions = 0;
    bool m_running = false;
};
