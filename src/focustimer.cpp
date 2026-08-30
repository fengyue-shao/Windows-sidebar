#include "include/focustimer.h"

#include <QDateTime>
#include <QSettings>

FocusTimer::FocusTimer(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_completedSessions = settings.value(QStringLiteral("focus/completedSessions"), 0).toInt();
    const int storedMinutes = settings.value(QStringLiteral("focus/durationMinutes"), 25).toInt();
    m_durationSeconds = qBound(1, storedMinutes, 120) * 60;
    m_remainingSeconds = m_durationSeconds;
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &FocusTimer::tick);
}

int FocusTimer::remainingSeconds() const { return m_remainingSeconds; }
int FocusTimer::durationSeconds() const { return m_durationSeconds; }
bool FocusTimer::running() const { return m_running; }
int FocusTimer::completedSessions() const { return m_completedSessions; }

qreal FocusTimer::progress() const
{
    return m_durationSeconds > 0
        ? 1.0 - static_cast<qreal>(m_remainingSeconds) / m_durationSeconds
        : 0.0;
}

void FocusTimer::toggle()
{
    if (m_running) {
        tick();
        setRunning(false);
        return;
    }
    if (m_remainingSeconds <= 0)
        m_remainingSeconds = m_durationSeconds;
    m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + qint64(m_remainingSeconds) * 1000;
    setRunning(true);
}

void FocusTimer::reset()
{
    setRunning(false);
    if (m_remainingSeconds == m_durationSeconds)
        return;
    m_remainingSeconds = m_durationSeconds;
    emit remainingSecondsChanged();
}

void FocusTimer::setDurationMinutes(int minutes)
{
    minutes = qBound(1, minutes, 120);
    const int duration = minutes * 60;
    if (m_durationSeconds == duration && m_remainingSeconds == duration)
        return;
    setRunning(false);
    m_durationSeconds = duration;
    m_remainingSeconds = duration;
    QSettings().setValue(QStringLiteral("focus/durationMinutes"), minutes);
    emit durationChanged();
    emit remainingSecondsChanged();
}

void FocusTimer::clearCompletedSessions()
{
    if (m_completedSessions == 0)
        return;
    m_completedSessions = 0;
    QSettings().setValue(QStringLiteral("focus/completedSessions"), 0);
    emit completedSessionsChanged();
}

void FocusTimer::tick()
{
    if (!m_running)
        return;
    const qint64 millisecondsLeft = m_deadlineMs - QDateTime::currentMSecsSinceEpoch();
    const int secondsLeft = millisecondsLeft > 0
        ? static_cast<int>((millisecondsLeft + 999) / 1000)
        : 0;
    if (secondsLeft != m_remainingSeconds) {
        m_remainingSeconds = secondsLeft;
        emit remainingSecondsChanged();
    }
    if (secondsLeft > 0)
        return;
    setRunning(false);
    ++m_completedSessions;
    QSettings().setValue(QStringLiteral("focus/completedSessions"), m_completedSessions);
    emit completedSessionsChanged();
    emit sessionCompleted();
}

void FocusTimer::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    if (running)
        m_timer.start();
    else
        m_timer.stop();
    emit runningChanged();
}
