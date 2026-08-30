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

fire_and_forget loadBluetoothDevices(QPointer<BluetoothManager> manager)
{
    QVariantList entries;
    QString status;
    try {
        const auto properties = single_threaded_vector<hstring>({
            L"System.Devices.Aep.IsConnected",
            L"System.Devices.Aep.SignalStrength"
        });
        const hstring selectors[] = {
            BluetoothDevice::GetDeviceSelectorFromPairingState(true),
            BluetoothDevice::GetDeviceSelectorFromPairingState(false),
            BluetoothLEDevice::GetDeviceSelectorFromPairingState(true),
            BluetoothLEDevice::GetDeviceSelectorFromPairingState(false)
        };
        std::array<Windows::Foundation::IAsyncOperation<DeviceInformationCollection>, 4> searches;
        for (size_t index = 0; index < searches.size(); ++index)
            searches[index] = DeviceInformation::FindAllAsync(selectors[index], properties, DeviceInformationKind::AssociationEndpoint);
        for (auto &search : searches) {
            const auto devices = co_await search;
            for (const auto &device : devices) {
                const QString id = QString::fromStdWString(device.Id().c_str());
                bool duplicate = false;
                for (const auto &entry : entries) {
                    if (entry.toMap().value(QStringLiteral("id")).toString() == id) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate || device.Name().empty())
                    continue;
                QVariantMap entry;
                entry.insert(QStringLiteral("id"), id);
                entry.insert(QStringLiteral("name"), QString::fromStdWString(device.Name().c_str()));
                entry.insert(QStringLiteral("paired"), device.Pairing().IsPaired());
                entry.insert(QStringLiteral("canPair"), device.Pairing().CanPair());
                const auto connectedValue = device.Properties().TryLookup(L"System.Devices.Aep.IsConnected");
                entry.insert(QStringLiteral("connected"), connectedValue && unbox_value<bool>(connectedValue));
                const auto signalValue = device.Properties().TryLookup(L"System.Devices.Aep.SignalStrength");
                entry.insert(QStringLiteral("signal"), signalValue ? unbox_value<int32_t>(signalValue) : -100);
                entries.append(entry);
            }
        }
        std::sort(entries.begin(), entries.end(), [](const QVariant &a, const QVariant &b) {
            const auto left = a.toMap();
            const auto right = b.toMap();
            if (left.value(QStringLiteral("connected")).toBool() != right.value(QStringLiteral("connected")).toBool())
                return left.value(QStringLiteral("connected")).toBool();
            if (left.value(QStringLiteral("paired")).toBool() != right.value(QStringLiteral("paired")).toBool())
                return left.value(QStringLiteral("paired")).toBool();
            return left.value(QStringLiteral("name")).toString().localeAwareCompare(right.value(QStringLiteral("name")).toString()) < 0;
        });
        status = entries.isEmpty() ? QStringLiteral("未发现蓝牙设备，请确认蓝牙已开启")
                                   : QStringLiteral("发现 %1 个蓝牙设备").arg(entries.size());
    } catch (const hresult_error &error) {
        status = QStringLiteral("蓝牙扫描失败：") + QString::fromStdWString(error.message().c_str());
    }
    QMetaObject::invokeMethod(qGuiApp, [manager, entries, status] {
        if (manager) manager->applyDevices(entries, status);
    }, Qt::QueuedConnection);
}

fire_and_forget changePairing(QPointer<BluetoothManager> manager, QString deviceId, bool pair)
{
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
    QMetaObject::invokeMethod(qGuiApp, [manager, status] {
        if (manager) manager->applyOperation(status);
    }, Qt::QueuedConnection);
}
}

BluetoothManager::BluetoothManager(QObject *parent) : QObject(parent) {}

void BluetoothManager::scan()
{
    if (m_scanning) return;
    m_scanning = true;
    m_statusText = QStringLiteral("正在扫描蓝牙设备…");
    emit scanningChanged();
    emit statusChanged();
    loadBluetoothDevices(this);
}

void BluetoothManager::pairDevice(const QString &deviceId)
{
    m_statusText = QStringLiteral("正在发起配对，请留意设备确认提示…");
    emit statusChanged();
    changePairing(this, deviceId, true);
}

void BluetoothManager::unpairDevice(const QString &deviceId)
{
    changePairing(this, deviceId, false);
}

void BluetoothManager::applyDevices(const QVariantList &devices, const QString &status)
{
    m_devices = devices;
    m_statusText = status;
    m_scanning = false;
    emit devicesChanged();
    emit statusChanged();
    emit scanningChanged();
}

void BluetoothManager::applyOperation(const QString &status)
{
    m_statusText = status;
    emit statusChanged();
    QTimer::singleShot(1500, this, &BluetoothManager::scan);
}
