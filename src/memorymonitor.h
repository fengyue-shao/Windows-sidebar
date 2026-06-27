#pragma once
#ifndef MEMORYMONITOR_H
#define MEMORYMONITOR_H
#include "monitorbase.h"
class MemoryMonitor:public MonitorBase
{
    Q_OBJECT
    Q_PROPERTY(double usage
               READ usage
               NOTIFY usageChanged)
public:
    explicit MemoryMonitor(QObject* parent=nullptr);
    double usage() const;
signals:
    void usageChanged();
protected:
    void updateData() override;
private:
    double m_usage=0.0;
};

#endif // MEMORYMONITOR_H
