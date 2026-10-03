#include "include/bluetoothmanager.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <array>
#include <algorithm>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;

namespace {
QString pairingStatusText(DevicePairingResultStatus status)
{
    switch (status) {
    case DevicePairingResultStatus::Paired:
    case DevicePairingResultStatus::AlreadyPaired:
        return QStringLiteral("设备已配对；支持的服务将自动连接");
    case DevicePairingResultStatus::NotReadyToPair:
        return QStringLiteral("设备尚未进入配对模式");
    case DevicePairingResultStatus::AuthenticationFailure:
        return QStringLiteral("配对验证失败，请确认设备上的提示");
    case DevicePairingResultStatus::ConnectionRejected:
        return QStringLiteral("设备拒绝了配对请求");
    case DevicePairingResultStatus::TooManyConnections:
        return QStringLiteral("设备连接数已满");
    case DevicePairingResultStatus::PairingCanceled:
        return QStringLiteral("配对已取消");
    default:
        return QStringLiteral("配对失败（状态 %1）").arg(static_cast<int>(status));
    }
}

QVariantMap deviceRow(const DeviceInformation &device)
{
    const auto connected = device.Properties().TryLookup(L"System.Devices.Aep.IsConnected");
    const auto signal = device.Properties().TryLookup(L"System.Devices.Aep.SignalStrength");
    return {{"id", QString::fromStdWString(device.Id().c_str())},
        {"name", QString::fromStdWString(device.Name().c_str())},
        {"paired", device.Pairing().IsPaired()}, {"canPair", device.Pairing().CanPair()},
        {"connected", connected && unbox_value_or<bool>(connected, false)},
        {"signal", signal ? unbox_value_or<int32_t>(signal, -100) : -100}};
}

Windows::Foundation::IAsyncAction changePairing(QPointer<BluetoothManager> manager, QString deviceId, bool pair, quint64 generation)
{
    auto cancellation = co_await get_cancellation_token();
    cancellation.enable_propagation();
    QString status;
    try {
        const auto device = co_await DeviceInformation::CreateFromIdAsync(deviceId.toStdWString());
        if (!device) {
            status = QStringLiteral("设备已不可用");
        } else if (pair) {
            const auto result = co_await device.Pairing().PairAsync();
            status = pairingStatusText(result.Status());
        } else {
            const auto result = co_await device.Pairing().UnpairAsync();
            status = result.Status() == DeviceUnpairingResultStatus::Unpaired
                || result.Status() == DeviceUnpairingResultStatus::AlreadyUnpaired
                ? QStringLiteral("设备已取消配对")
                : QStringLiteral("取消配对失败（状态 %1）").arg(static_cast<int>(result.Status()));
        }
    } catch (const hresult_error &error) {
        status = QStringLiteral("蓝牙操作失败：") + QString::fromStdWString(error.message().c_str());
    }
    QMetaObject::invokeMethod(qGuiApp, [manager, status, generation] {
        if (manager) manager->applyOperation(status, generation);
    }, Qt::QueuedConnection);
}
}

BluetoothManager::BluetoothManager(QObject *parent) : QObject(parent) {}
BluetoothManager::~BluetoothManager() { cancelScan(); cancelOperation(); }

void BluetoothManager::scan()
{
    if (m_scanning || m_pairing) return;
    m_scanning = true;
    m_statusText = QStringLiteral("正在扫描蓝牙设备…");
    emit scanningChanged();
    emit statusChanged();
    const auto generation = ++m_scanGeneration;
    m_previousRows.clear();
    for (const auto &entry : m_devices) {
        const auto row = entry.toMap(); m_previousRows.insert(row.value("id").toString(), row);
    }
    for (auto &slot : m_watchers) { slot.information.clear(); slot.rows.clear(); slot.complete = false; }
    try {
        const auto properties = single_threaded_vector<hstring>({L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.SignalStrength"});
        const hstring selectors[] = {BluetoothDevice::GetDeviceSelectorFromPairingState(true),
            BluetoothDevice::GetDeviceSelectorFromPairingState(false),
            BluetoothLEDevice::GetDeviceSelectorFromPairingState(true), BluetoothLEDevice::GetDeviceSelectorFromPairingState(false)};
        const QPointer<BluetoothManager> owner(this);
        for (size_t source = 0; source < m_watchers.size(); ++source) {
            auto &slot = m_watchers[source];
            slot.watcher = DeviceInformation::CreateWatcher(selectors[source], properties, DeviceInformationKind::AssociationEndpoint);
            slot.added = slot.watcher.Added([owner, source, generation](auto const &, DeviceInformation const &device) {
                QMetaObject::invokeMethod(qGuiApp, [owner, source, generation, device] {
                    if (!owner || !owner->accepts(source, generation)) return;
                    try {
                        owner->m_watchers[source].information.insert(QString::fromStdWString(device.Id().c_str()), device);
                        owner->acceptDevice(source, deviceRow(device), generation);
                    } catch (...) { owner->watcherFailed(generation, QStringLiteral("无法读取蓝牙设备信息")); }
                }, Qt::QueuedConnection);
            });
            slot.updated = slot.watcher.Updated([owner, source, generation](auto const &, DeviceInformationUpdate const &update) {
                QMetaObject::invokeMethod(qGuiApp, [owner, source, generation, update] {
                    if (!owner || !owner->accepts(source, generation)) return;
                    try {
                        auto &information = owner->m_watchers[source].information;
                        auto found = information.find(QString::fromStdWString(update.Id().c_str()));
                        if (found == information.end()) return;
                        found.value().Update(update);
                        owner->acceptDevice(source, deviceRow(found.value()), generation);
                    } catch (...) { owner->watcherFailed(generation, QStringLiteral("无法更新蓝牙设备信息")); }
                }, Qt::QueuedConnection);
            });
            slot.removed = slot.watcher.Removed([owner, source, generation](auto const &, DeviceInformationUpdate const &update) {
                const auto id = QString::fromStdWString(update.Id().c_str());
                QMetaObject::invokeMethod(qGuiApp, [owner, source, generation, id] { if (owner) owner->removeDevice(source, id, generation); }, Qt::QueuedConnection);
            });
            slot.enumerated = slot.watcher.EnumerationCompleted([owner, source, generation](auto const &, auto const &) {
                QMetaObject::invokeMethod(qGuiApp, [owner, source, generation] { if (owner) owner->enumerationComplete(source, generation); }, Qt::QueuedConnection);
            });
            slot.stopped = slot.watcher.Stopped([owner, generation](auto const &, auto const &) {
                QMetaObject::invokeMethod(qGuiApp, [owner, generation] {
                    if (owner) owner->watcherFailed(generation, QStringLiteral("Windows 已停止蓝牙发现，请检查无线状态后重试"));
                }, Qt::QueuedConnection);
            });
        }
        for (auto &slot : m_watchers) slot.watcher.Start();
    } catch (const hresult_error &error) {
        watcherFailed(generation, QStringLiteral("蓝牙发现失败：") + QString::fromStdWString(error.message().c_str()));
    } catch (...) { watcherFailed(generation, QStringLiteral("无法启动蓝牙发现")); }
    QTimer::singleShot(30000, this, [this, generation] { finishScan(generation); });
}

void BluetoothManager::cancelScan()
{
    ++m_scanGeneration;
    stopWatchers();
    if (!m_scanning) return;
    m_scanning = false; m_statusText = QStringLiteral("已停止扫描等待，保留已有列表"); emit scanningChanged(); emit statusChanged();
}
void BluetoothManager::cancelOperation()
{
    ++m_pairGeneration;
    try { if (m_pairAction) m_pairAction.Cancel(); } catch (...) {}
    m_pairAction = nullptr;
    if (!m_pairing) return;
    m_pairing = false; m_statusText = QStringLiteral("已结束配对操作等待；系统状态可能已改变，请刷新核验"); emit busyChanged(); emit statusChanged();
}
void BluetoothManager::beginPairing(const QString &id, bool pair)
{
    if (m_pairing || id.isEmpty()) return;
    cancelScan();
    m_pairing = true; const auto generation = ++m_pairGeneration;
    m_statusText = pair ? QStringLiteral("正在发起配对，请留意 Windows 或设备确认提示…") : QStringLiteral("正在取消配对…");
    emit busyChanged(); emit statusChanged();
    m_pairAction = changePairing(this, id, pair, generation);
    QTimer::singleShot(45000, this, [this, generation] {
        if (!m_pairing || generation != m_pairGeneration) return;
        cancelOperation(); m_statusText = QStringLiteral("配对操作等待超时；不保证系统操作已撤回，请刷新核验"); emit statusChanged();
    });
}

void BluetoothManager::pairDevice(const QString &deviceId)
{
    beginPairing(deviceId, true);
}

void BluetoothManager::unpairDevice(const QString &deviceId)
{
    beginPairing(deviceId, false);
}

void BluetoothManager::stopWatchers()
{
    for (auto &slot : m_watchers) {
        if (!slot.watcher) continue;
        // Each subscription can be absent after a partially failed start.
        try { if (slot.added.value) slot.watcher.Added(slot.added); } catch (...) {}
        try { if (slot.updated.value) slot.watcher.Updated(slot.updated); } catch (...) {}
        try { if (slot.removed.value) slot.watcher.Removed(slot.removed); } catch (...) {}
        try { if (slot.enumerated.value) slot.watcher.EnumerationCompleted(slot.enumerated); } catch (...) {}
        try { if (slot.stopped.value) slot.watcher.Stopped(slot.stopped); } catch (...) {}
        try {
            const auto state = slot.watcher.Status();
            if (state == DeviceWatcherStatus::Started || state == DeviceWatcherStatus::EnumerationCompleted) slot.watcher.Stop();
        } catch (...) {}
        slot.watcher = nullptr;
        slot.added = {}; slot.updated = {}; slot.removed = {}; slot.enumerated = {}; slot.stopped = {};
        slot.information.clear();
    }
}

bool BluetoothManager::accepts(size_t source, quint64 generation) const
{
    return m_scanning && generation == m_scanGeneration && source < m_watchers.size();
}

void BluetoothManager::acceptDevice(size_t source, const QVariantMap &row, quint64 generation)
{
    if (!accepts(source, generation)) return;
    const auto id = row.value("id").toString();
    if (id.isEmpty()) return;
    if (row.value("name").toString().isEmpty()) m_watchers[source].rows.remove(id);
    else m_watchers[source].rows.insert(id, row);
    m_previousRows.remove(id);
    publishDevices();
}

void BluetoothManager::removeDevice(size_t source, const QString &id, quint64 generation)
{
    if (!accepts(source, generation)) return;
    m_watchers[source].information.remove(id); m_watchers[source].rows.remove(id);
    m_previousRows.remove(id);
    publishDevices();
}

void BluetoothManager::enumerationComplete(size_t source, quint64 generation)
{
    if (!accepts(source, generation)) return;
    m_watchers[source].complete = true;
    if (std::all_of(m_watchers.begin(), m_watchers.end(), [](const auto &slot) { return slot.complete; })) m_previousRows.clear();
    publishDevices(); // EnumerationCompleted does NOT end the discovery window.
}

void BluetoothManager::publishDevices()
{
    auto rows = m_previousRows;
    for (const auto &slot : m_watchers) for (auto it = slot.rows.cbegin(); it != slot.rows.cend(); ++it) rows.insert(it.key(), it.value());
    QVariantList entries;
    for (auto it = rows.cbegin(); it != rows.cend(); ++it) entries.append(it.value());
    std::sort(entries.begin(), entries.end(), [](const QVariant &a, const QVariant &b) {
        const auto left = a.toMap(), right = b.toMap();
        for (const auto &key : {QStringLiteral("connected"), QStringLiteral("paired")})
            if (left.value(key).toBool() != right.value(key).toBool()) return left.value(key).toBool();
        const auto nameOrder = left.value("name").toString().localeAwareCompare(right.value("name").toString());
        return nameOrder ? nameOrder < 0 : left.value("id").toString() < right.value("id").toString();
    });
    if (m_devices != entries) { m_devices = entries; emit devicesChanged(); }
    const bool complete = std::all_of(m_watchers.begin(), m_watchers.end(), [](const auto &slot) { return slot.complete; });
    m_statusText = complete ? QStringLiteral("初始枚举完成 · %1 个设备 · 继续发现至扫描窗口结束").arg(entries.size())
        : QStringLiteral("正在发现蓝牙设备 · 已显示 %1 个（包含尚待核验的旧列表）").arg(entries.size());
    emit statusChanged();
}

void BluetoothManager::watcherFailed(quint64 generation, const QString &message)
{
    if (!m_scanning || generation != m_scanGeneration) return;
    cancelScan(); m_statusText = message + QStringLiteral("；保留已显示列表"); emit statusChanged();
}

void BluetoothManager::finishScan(quint64 generation)
{
    if (!m_scanning || generation != m_scanGeneration) return;
    const bool complete = std::all_of(m_watchers.begin(), m_watchers.end(), [](const auto &slot) { return slot.complete; });
    cancelScan();
    m_statusText = complete ? QStringLiteral("扫描结束 · %1 个蓝牙设备").arg(m_devices.size())
        : QStringLiteral("扫描窗口结束，初始枚举未全部完成；保留已显示列表，可刷新重试");
    emit statusChanged();
}

void BluetoothManager::applyOperation(const QString &status, quint64 generation)
{
    if (!m_pairing || generation != m_pairGeneration) return;
    m_pairAction = nullptr; m_pairing = false; emit busyChanged();
    m_statusText = status;
    emit statusChanged();
    QTimer::singleShot(1500, this, &BluetoothManager::scan);
}
