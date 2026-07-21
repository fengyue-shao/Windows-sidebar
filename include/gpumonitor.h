#pragma once
#ifndef GPUMONITOR_H
#define GPUMONITOR_H

#include "monitorbase.h"

#include <pdh.h>
#include <pdhmsg.h>

class GpuMonitor : public MonitorBase
{
    Q_OBJECT
    Q_PROPERTY(double usage READ usage NOTIFY usageChanged)

public:
    explicit GpuMonitor(QObject *parent = nullptr);
    ~GpuMonitor() override;

    double usage() const;

signals:
    void usageChanged();

protected:
    void updateData() override;

private:
    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_counter = nullptr;
    double m_usage = 0.0;
    bool m_hasInitialSample = false;
};

#endif // GPUMONITOR_H
