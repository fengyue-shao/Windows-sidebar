#pragma once
#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include <QObject>
#include <QTimer>
class CpuMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double usage
                   READ usage
                       NOTIFY usageChanged)

public:
    explicit CpuMonitor(QObject *parent = nullptr);

    double usage() const;

signals:
    void usageChanged();

private:
    void updateUsage();
private:
    double m_usage = 0.0;

    QTimer m_timer;
    quint64 m_lastIdle=0;
    quint64 m_lastKernel=0;
    quint64 m_lastUser=0;
};

#endif // CPUMONITOR_H
