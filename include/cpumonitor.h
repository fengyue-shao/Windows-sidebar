#pragma once
#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "monitorbase.h"

class CpuMonitor : public MonitorBase
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

protected:
    void updateUsage();
    void updateData() override;
private:
    double m_usage = 0.0;

    quint64 m_lastIdle=0;
    quint64 m_lastKernel=0;
    quint64 m_lastUser=0;
};

#endif // CPUMONITOR_H
