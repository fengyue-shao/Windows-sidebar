#include "include/gpumonitor.h"

#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <algorithm>
#include <dxgi1_2.h>

GpuMonitor::GpuMonitor(QObject *parent)
    : MonitorBase(parent)
{
    enumerateHardwareAdapters();

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

QVariantList GpuMonitor::adapters() const
{
    return m_adapters;
}

void GpuMonitor::enumerateHardwareAdapters()
{
    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                  reinterpret_cast<void **>(&factory))))
        return;

    for (UINT index = 0;; ++index) {
        IDXGIAdapter1 *adapter = nullptr;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND)
            break;
        if (!adapter)
            continue;

        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description))
            && !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            const QString key = QStringLiteral("luid_0x%1_0x%2")
                                    .arg(static_cast<quint32>(description.AdapterLuid.HighPart),
                                         8, 16, QLatin1Char('0'))
                                    .arg(description.AdapterLuid.LowPart,
                                         8, 16, QLatin1Char('0'))
                                    .toLower();
            m_adapterOrder.append(key);
            m_adapterNames.insert(
                key, QString::fromWCharArray(description.Description).trimmed());
        }
        adapter->Release();
    }
    factory->Release();
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

    struct AdapterUsage {
        QHash<QString, double> usageByEngineType;
    };
    QHash<QString, AdapterUsage> usageByAdapter;
    const QRegularExpression adapterPattern(
        QStringLiteral("(luid_0x[0-9a-f]+_0x[0-9a-f]+)_phys_\\d+"),
        QRegularExpression::CaseInsensitiveOption);
    for (DWORD i = 0; i < itemCount; ++i) {
        if (items[i].FmtValue.CStatus != ERROR_SUCCESS)
            continue;

        const QString instance = QString::fromWCharArray(items[i].szName);
        const QRegularExpressionMatch adapterMatch = adapterPattern.match(instance);
        if (!adapterMatch.hasMatch())
            continue;
        const QString adapterKey = adapterMatch.captured(1).toLower();
        if (!m_adapterNames.isEmpty() && !m_adapterNames.contains(adapterKey))
            continue;
        AdapterUsage &adapter = usageByAdapter[adapterKey];
        const qsizetype enginePosition = instance.indexOf("engtype_", 0, Qt::CaseInsensitive);
        const QString engineType = enginePosition >= 0
                                       ? instance.mid(enginePosition).toLower()
                                       : instance.toLower();
        adapter.usageByEngineType[engineType] +=
            qMax(0.0, items[i].FmtValue.doubleValue);
    }

    struct AdapterResult {
        QString key;
        int order;
        double usage;
    };
    QVector<AdapterResult> results;
    double newUsage = 0.0;
    for (auto adapterIt = usageByAdapter.cbegin(); adapterIt != usageByAdapter.cend(); ++adapterIt) {
        double adapterUsage = 0.0;
        for (auto engineIt = adapterIt->usageByEngineType.cbegin();
             engineIt != adapterIt->usageByEngineType.cend(); ++engineIt) {
            adapterUsage = qMax(adapterUsage, engineIt.value());
        }
        adapterUsage = qBound(0.0, adapterUsage, 100.0);
        int order = m_adapterOrder.indexOf(adapterIt.key());
        if (order < 0)
            order = m_adapterOrder.size() + results.size();
        results.append({adapterIt.key(), order, adapterUsage});
        newUsage = qMax(newUsage, adapterUsage);
    }

    std::sort(results.begin(), results.end(), [](const AdapterResult &left,
                                                  const AdapterResult &right) {
        if (left.order != right.order)
            return left.order < right.order;
        return left.key < right.key;
    });

    QVariantList newAdapters;
    for (qsizetype index = 0; index < results.size(); ++index) {
        const AdapterResult &result = results.at(index);
        QVariantMap adapter;
        adapter.insert(QStringLiteral("id"), result.key);
        adapter.insert(QStringLiteral("name"), QStringLiteral("GPU %1").arg(index));
        adapter.insert(QStringLiteral("description"), m_adapterNames.value(result.key));
        adapter.insert(QStringLiteral("usage"), result.usage);
        newAdapters.append(adapter);
    }

    if (newAdapters != m_adapters) {
        m_adapters = newAdapters;
        emit adaptersChanged();
    }

    newUsage = qBound(0.0, newUsage, 100.0);
    if (qFuzzyCompare(newUsage, m_usage))
        return;

    m_usage = newUsage;
    emit usageChanged();
}
