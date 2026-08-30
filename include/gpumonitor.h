#pragma once
#ifndef GPUMONITOR_H
#define GPUMONITOR_H

#include "monitorbase.h"

#include <pdh.h>
#include <pdhmsg.h>
#include <QHash>
#include <QStringList>
#include <QVariantList>

class GpuMonitor : public MonitorBase
{
    Q_OBJECT
    Q_PROPERTY(double usage READ usage NOTIFY usageChanged)
    Q_PROPERTY(QVariantList adapters READ adapters NOTIFY adaptersChanged)

public:
    explicit GpuMonitor(QObject *parent = nullptr);
    ~GpuMonitor() override;

    double usage() const;
    QVariantList adapters() const;

signals:
    void usageChanged();
    void adaptersChanged();

protected:
    void updateData() override;

private:
    void enumerateHardwareAdapters();

    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_counter = nullptr;
    double m_usage = 0.0;
    QVariantList m_adapters;
    QHash<QString, QString> m_adapterNames;
    QStringList m_adapterOrder;
    bool m_hasInitialSample = false;
};

#endif // GPUMONITOR_H
