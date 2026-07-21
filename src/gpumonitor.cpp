#include "include/gpumonitor.h"

#include <QHash>
#include <QString>
#include <QVector>

GpuMonitor::GpuMonitor(QObject *parent)
    : MonitorBase(parent)
{
    if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS)
        return;

    const PDH_STATUS status = PdhAddEnglishCounterW(
        m_query,
        L"\\GPU Engine(*)\\Utilization Percentage",
        0,
        &m_counter);

    if (status != ERROR_SUCCESS) {
        PdhCloseQuery(m_query);
        m_query = nullptr;
        return;
    }

    updateData();
}

GpuMonitor::~GpuMonitor()
{
    if (m_query)
        PdhCloseQuery(m_query);
}

double GpuMonitor::usage() const
{
    return m_usage;
}

void GpuMonitor::updateData()
{
    if (!m_query || !m_counter)
        return;

    if (PdhCollectQueryData(m_query) != ERROR_SUCCESS)
        return;

    if (!m_hasInitialSample) {
        m_hasInitialSample = true;
        return;
    }

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(
        m_counter,
        PDH_FMT_DOUBLE,
        &bufferSize,
        &itemCount,
        nullptr);

    if (status != PDH_MORE_DATA || bufferSize == 0)
        return;

    QVector<unsigned char> buffer(static_cast<qsizetype>(bufferSize));
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());

    status = PdhGetFormattedCounterArrayW(
        m_counter,
        PDH_FMT_DOUBLE,
        &bufferSize,
        &itemCount,
        items);
    if (status != ERROR_SUCCESS)
        return;

    QHash<QString, double> usageByEngineType;
    for (DWORD i = 0; i < itemCount; ++i) {
        if (items[i].FmtValue.CStatus != ERROR_SUCCESS)
            continue;

        const QString instance = QString::fromWCharArray(items[i].szName);
        const qsizetype enginePosition = instance.indexOf("engtype_", 0, Qt::CaseInsensitive);
        const QString engineType = enginePosition >= 0
                                       ? instance.mid(enginePosition).toLower()
                                       : instance.toLower();
        usageByEngineType[engineType] += qMax(0.0, items[i].FmtValue.doubleValue);
    }

    double newUsage = 0.0;
    for (auto it = usageByEngineType.cbegin(); it != usageByEngineType.cend(); ++it)
        newUsage = qMax(newUsage, it.value());

    newUsage = qBound(0.0, newUsage, 100.0);
    if (qFuzzyCompare(newUsage, m_usage))
        return;

    m_usage = newUsage;
    emit usageChanged();
}
