#include "include/memorymonitor.h"

#include <windows.h>

MemoryMonitor::MemoryMonitor(QObject *parent)
    : MonitorBase(parent)
{
    MemoryMonitor::updateData();

}

double MemoryMonitor::usage() const
{
    return m_usage;
}

void MemoryMonitor::updateData()
{
    MEMORYSTATUSEX memInfo;

    memInfo.dwLength =
        sizeof(MEMORYSTATUSEX);

    if (!GlobalMemoryStatusEx(
            &memInfo))
    {
        return;
    }

    const quint64 total =
        memInfo.ullTotalPhys;

    const quint64 available =
        memInfo.ullAvailPhys;

    const quint64 used =
        total - available;

    const double newUsage =
        static_cast<double>(used)
        * 100.0
        / total;

    if (qFuzzyCompare(
            newUsage,
            m_usage))
    {
        return;
    }

    m_usage = newUsage;

    emit usageChanged();
}
