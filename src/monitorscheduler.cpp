#include "include/monitorscheduler.h"

#include <numeric>

MonitorScheduler::MonitorScheduler(QObject *parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MonitorScheduler::dispatch);
}

void MonitorScheduler::start()
{
    if (m_modules.isEmpty())
        return;

    m_timer.start(baseInterval());
}

void MonitorScheduler::stop()
{
    m_timer.stop();
}

void MonitorScheduler::dispatch()
{
    const int elapsed = m_timer.interval();
    m_elapsedMilliseconds += elapsed;
    emit tick(m_elapsedMilliseconds);

    for (qsizetype index = m_modules.size() - 1; index >= 0; --index) {
        Entry &entry = m_modules[index];
        if (!entry.owner) {
            m_modules.removeAt(index);
            continue;
        }

        entry.elapsedMs += elapsed;
        if (entry.elapsedMs < entry.intervalMs)
            continue;

        entry.elapsedMs %= entry.intervalMs;
        entry.module->refresh();
    }
}

int MonitorScheduler::baseInterval() const
{
    int interval = 0;
    for (const Entry &entry : m_modules)
        interval = interval == 0 ? entry.intervalMs : std::gcd(interval, entry.intervalMs);
    return qMax(50, interval);
}
