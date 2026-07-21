#pragma once
#ifndef MONITORSCHEDULER_H
#define MONITORSCHEDULER_H

#include "periodicmodule.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <type_traits>

class MonitorScheduler : public QObject
{
    Q_OBJECT

public:
    explicit MonitorScheduler(QObject *parent = nullptr);

    template<typename T>
    void addModule(T *module, int intervalMs)
    {
        static_assert(std::is_base_of_v<QObject, T>);
        static_assert(std::is_base_of_v<PeriodicModule, T>);

        if (!module || intervalMs <= 0)
            return;

        m_modules.append({module, module, intervalMs, 0});
    }

    void start();
    void stop();

signals:
    void tick(qint64 elapsedMilliseconds);

private:
    struct Entry {
        QPointer<QObject> owner;
        PeriodicModule *module = nullptr;
        int intervalMs = 1000;
        int elapsedMs = 0;
    };

    void dispatch();
    int baseInterval() const;

    QVector<Entry> m_modules;
    QTimer m_timer;
    qint64 m_elapsedMilliseconds = 0;
};

#endif // MONITORSCHEDULER_H
