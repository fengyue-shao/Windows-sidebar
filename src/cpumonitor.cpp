#include "include/CpuMonitor.h"

#include <windows.h>

CpuMonitor::CpuMonitor(QObject *parent)
    : QObject(parent)
{
    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    GetSystemTimes(
        &idleTime,
        &kernelTime,
        &userTime
        );
    auto fileTimeToUInt64=[](const FILETIME &ft){
        ULARGE_INTEGER li;
        li.LowPart=ft.dwLowDateTime;
        li.HighPart=ft.dwHighDateTime;
        return li.QuadPart;
    };
    m_lastIdle=fileTimeToUInt64(idleTime);
    m_lastKernel=fileTimeToUInt64(kernelTime);
    m_lastUser=fileTimeToUInt64(userTime);
    connect(&m_timer,
            &QTimer::timeout,
            this,
            &CpuMonitor::updateUsage
            );

    m_timer.start(1000);
}

double CpuMonitor::usage() const
{
    return m_usage;
}

void CpuMonitor::updateUsage(){
    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if(!GetSystemTimes(&idleTime,&kernelTime,&userTime)){
        return;
    }
    auto fileTimeToUInt64=[](const FILETIME &ft){
        ULARGE_INTEGER li;
        li.LowPart=ft.dwLowDateTime;
        li.HighPart=ft.dwHighDateTime;
        return li.QuadPart;
    };
    quint64 idle =
        fileTimeToUInt64(idleTime);

    quint64 kernel =
        fileTimeToUInt64(kernelTime);

    quint64 user =
        fileTimeToUInt64(userTime);

    quint64 idleDiff =
        idle - m_lastIdle;

    quint64 kernelDiff =
        kernel - m_lastKernel;

    quint64 userDiff =
        user - m_lastUser;

    quint64 total =
        kernelDiff + userDiff;

    if (total > 0)
    {
        m_usage =
            100.0 *
            (total - idleDiff)
            / total;

        emit usageChanged();
    }

    m_lastIdle = idle;
    m_lastKernel = kernel;
    m_lastUser = user;
}
