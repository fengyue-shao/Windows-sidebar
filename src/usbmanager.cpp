#include "include/usbmanager.h"
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <winioctl.h>
#include <winrt/base.h>
#include <vector>

namespace {
const GUID diskInterface = {0x53f56307, 0xb6bf, 0x11d0, {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b}};
const GUID efiPartition = {0xc12a7328, 0xf81f, 0x11d2, {0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b}};
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Device { DEVINST node = 0; QString id, name; QStringList volumes; bool protectedDisk = false; };
struct DeviceInfoSet {
    HDEVINFO value;
    ~DeviceInfoSet() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
};
QString vetoReason(PNP_VETO_TYPE veto)
{
    switch (veto) {
    case PNP_VetoOutstandingOpen: return QStringLiteral("文件或句柄仍被占用");
    case PNP_VetoPendingClose: return QStringLiteral("正在等待关闭文件");
    case PNP_VetoWindowsApp: return QStringLiteral("应用程序阻止移除");
    case PNP_VetoWindowsService: return QStringLiteral("系统服务阻止移除");
    case PNP_VetoDevice: return QStringLiteral("设备阻止移除");
    case PNP_VetoDriver: return QStringLiteral("驱动程序阻止移除");
    case PNP_VetoInsufficientRights: return QStringLiteral("权限不足");
    case PNP_VetoNonDisableable: return QStringLiteral("设备不能被禁用");
    default: return QStringLiteral("系统或设备拒绝移除（%1）").arg(static_cast<int>(veto));
    }
}
QString property(DEVINST node, ULONG key)
{
    wchar_t value[1024]{}; ULONG size = sizeof(value), type = 0;
    if (CM_Get_DevNode_Registry_PropertyW(node, key, &type, value, &size, 0) != CR_SUCCESS || type != REG_SZ) return {};
    return QString::fromWCharArray(value);
}
QString deviceId(DEVINST node)
{
    wchar_t id[MAX_DEVICE_ID_LEN]{};
    return CM_Get_Device_IDW(node, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS ? QString::fromWCharArray(id) : QString();
}
bool removable(DEVINST node)
{
    ULONG caps = 0, size = sizeof(caps), type = 0;
    return CM_Get_DevNode_Registry_PropertyW(node, CM_DRP_CAPABILITIES, &type, &caps, &size, 0) == CR_SUCCESS
        && type == REG_DWORD && (caps & CM_DEVCAP_REMOVABLE);
}
DEVINST storageParent(DEVINST node)
{
    // Never ascend to a USB hub/controller. Only the physical mass-storage
    // function with an explicit removable capability is an eject target.
    for (int depth = 0; depth < 8; ++depth) {
        const auto service = property(node, CM_DRP_SERVICE);
        const auto id = deviceId(node);
        if (UsbManager::isStorageFunction(id, service, removable(node))) return node;
        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) break;
        node = parent;
    }
    return 0;
}
bool diskNumber(HANDLE handle, DWORD &number)
{
    STORAGE_DEVICE_NUMBER result{}; DWORD bytes = 0;
    if (!DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &result, sizeof(result), &bytes, nullptr)
        || result.DeviceType != FILE_DEVICE_DISK) return false;
    number = result.DeviceNumber; return true;
}
bool hasBootPartition(HANDLE handle)
{
    // Unknown partition layouts are excluded, not assumed safe.
    std::vector<unsigned char> data(128 * 1024); DWORD bytes = 0;
    if (!DeviceIoControl(handle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, data.data(), static_cast<DWORD>(data.size()), &bytes, nullptr)) return true;
    if (bytes < offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) return true;
    const auto *layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX *>(data.data());
    if (layout->PartitionCount > (bytes - offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) / sizeof(PARTITION_INFORMATION_EX)) return true;
    for (DWORD index = 0; index < layout->PartitionCount; ++index) {
        const auto &partition = layout->PartitionEntry[index];
        if (partition.PartitionStyle == PARTITION_STYLE_MBR && partition.Mbr.BootIndicator) return true;
        if (partition.PartitionStyle == PARTITION_STYLE_GPT && IsEqualGUID(partition.Gpt.PartitionType, efiPartition)) return true;
    }
    return false;
}

QMap<QString, Device> enumerate(QString &status)
{
    QMap<DWORD, QStringList> volumes;
    QSet<DWORD> protectedDisks;
    wchar_t windowsPath[MAX_PATH]{}, windowsVolume[MAX_PATH]{};
    const bool systemKnown = GetSystemWindowsDirectoryW(windowsPath, MAX_PATH)
        && GetVolumePathNameW(windowsPath, windowsVolume, MAX_PATH);
    wchar_t drives[512]{};
    const auto driveLength = GetLogicalDriveStringsW(512, drives);
    if (!systemKnown || !driveLength || driveLength >= 512) {
        status = QStringLiteral("无法核验系统卷，已禁用弹出列表"); return {};
    }
    bool protectedSystemFound = false;
    for (auto *drive = drives; *drive; drive += wcslen(drive) + 1) {
        const auto driveType = GetDriveTypeW(drive);
        if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE) continue;
        const QString path = QStringLiteral("\\\\.\\%1").arg(QString::fromWCharArray(drive).left(2));
        Handle handle{CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)};
        DWORD number = 0;
        if (handle.value == INVALID_HANDLE_VALUE || !diskNumber(handle.value, number)) continue;
        volumes[number].append(QString::fromWCharArray(drive));
        if (_wcsicmp(drive, windowsVolume) == 0) { protectedDisks.insert(number); protectedSystemFound = true; }
    }
    if (!protectedSystemFound) { status = QStringLiteral("无法核验系统磁盘，已禁用弹出列表"); return {}; }
    const HDEVINFO info = SetupDiGetClassDevsW(&diskInterface, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    const DeviceInfoSet cleanup{info};
    if (info == INVALID_HANDLE_VALUE) { status = QStringLiteral("读取 USB 存储列表失败"); return {}; }
    QMap<QString, Device> groups;
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData{}; interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &diskInterface, index, &interfaceData)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) break;
            status = QStringLiteral("设备枚举不完整（错误 %1），已禁用弹出列表").arg(error);
            return {};
        }
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &interfaceData, nullptr, 0, &required, nullptr);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || required > 65536) {
            status = QStringLiteral("无法完整核验设备接口，已禁用弹出列表"); return {};
        }
        std::vector<unsigned char> buffer(required);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buffer.data()); detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA deviceInfo{}; deviceInfo.cbSize = sizeof(deviceInfo);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &interfaceData, detail, required, nullptr, &deviceInfo)) {
            status = QStringLiteral("设备接口已改变（错误 %1），请刷新后重试").arg(GetLastError()); return {};
        }
        const auto parent = storageParent(deviceInfo.DevInst);
        if (!parent) continue;
        const QString id = deviceId(parent);
        if (id.isEmpty()) continue;
        auto &group = groups[id]; group.node = parent; group.id = id;
        group.name = property(parent, CM_DRP_FRIENDLYNAME);
        if (group.name.isEmpty()) group.name = property(deviceInfo.DevInst, CM_DRP_FRIENDLYNAME);
        if (group.name.isEmpty()) group.name = QStringLiteral("USB 存储设备");
        Handle handle{CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)};
        if (handle.value == INVALID_HANDLE_VALUE) { group.protectedDisk = true; continue; }
        STORAGE_PROPERTY_QUERY query{}; query.PropertyId = StorageDeviceProperty; query.QueryType = PropertyStandardQuery;
        std::vector<unsigned char> descriptor(8192); DWORD bytes = 0, number = 0;
        if (!DeviceIoControl(handle.value, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), descriptor.data(), static_cast<DWORD>(descriptor.size()), &bytes, nullptr)
            || bytes < sizeof(STORAGE_DEVICE_DESCRIPTOR) || reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR *>(descriptor.data())->BusType != BusTypeUsb
            || !diskNumber(handle.value, number)) { group.protectedDisk = true; continue; }
        group.protectedDisk |= !UsbManager::safeDisk(true, true, protectedDisks.contains(number), hasBootPartition(handle.value));
        group.volumes.append(volumes.value(number));
    }
    for (auto it = groups.begin(); it != groups.end();) {
        // First version requires mounted drive letters so the user can identify
        // the entire physical unit, including multi-LUN volumes, in confirmation.
        if (it->protectedDisk || it->volumes.isEmpty()) it = groups.erase(it);
        else { it->volumes.removeDuplicates(); ++it; }
    }
    status = groups.isEmpty() ? QStringLiteral("未发现可安全核验的 USB 存储设备")
        : QStringLiteral("%1 个设备 · 弹出前请保存并关闭文件").arg(groups.size());
    return groups;
}

winrt::fire_and_forget run(QPointer<UsbManager> owner, QString ejectId)
{
    co_await winrt::resume_background();
    QString status;
    QVariantList list;
    int operationResult = ejectId.isEmpty() ? 0 : -1;
    try {
        const auto devices = enumerate(status);
        if (!ejectId.isEmpty()) {
            const auto it = devices.constFind(ejectId);
            if (it == devices.cend()) status = QStringLiteral("设备已改变或无法核验，未执行弹出，请刷新");
            else {
                PNP_VETO_TYPE veto = PNP_VetoTypeUnknown;
                wchar_t vetoName[MAX_PATH]{};
                const auto result = CM_Request_Device_EjectW(it->node, &veto, vetoName, MAX_PATH, 0);
                operationResult = result == CR_SUCCESS ? 1 : -1;
                status = result == CR_SUCCESS ? QStringLiteral("Windows 已接受安全弹出请求，请确认设备消失后再拔出")
                    : QStringLiteral("%1（代码 %2）%3；请关闭占用文件后重试")
                        .arg(vetoReason(veto)).arg(result).arg(vetoName[0] ? " · " + QString::fromWCharArray(vetoName) : QString());
            }
        }
        for (const auto &device : devices) {
            if (operationResult == 1 && device.id == ejectId) continue;
            list.append(QVariantMap{{"id", device.id}, {"name", device.name}, {"volumes", device.volumes.join(" · ")}});
        }
    } catch (...) { status = QStringLiteral("USB 存储设备操作失败，未强制移除"); }
    QMetaObject::invokeMethod(qGuiApp, [owner, list, status, operationResult] { if (owner) owner->finish(list, status, operationResult); }, Qt::QueuedConnection);
}
}

bool UsbManager::isStorageFunction(const QString &id, const QString &service, bool removable)
{
    return removable && id.startsWith("USB\\VID_", Qt::CaseInsensitive)
        && (service.compare("USBSTOR", Qt::CaseInsensitive) == 0 || service.compare("UASPStor", Qt::CaseInsensitive) == 0);
}
bool UsbManager::safeDisk(bool usb, bool numberKnown, bool systemDisk, bool bootOrUnknown)
{
    return usb && numberKnown && !systemDisk && !bootOrUnknown;
}

UsbManager::UsbManager(QObject *parent) : QObject(parent)
{
    QTimer::singleShot(0, this, &UsbManager::refresh);
}
void UsbManager::refresh()
{
    if (m_busy) return;
    m_busy = true; m_status = QStringLiteral("正在核验 USB 存储设备…"); emit changed(); run(this, {});
}
void UsbManager::eject(const QString &id)
{
    if (m_busy || id.isEmpty()) return;
    bool known = false;
    for (const auto &device : m_devices) if (device.toMap().value("id").toString() == id) known = true;
    if (!known) { m_status = QStringLiteral("设备不在当前列表中，请刷新"); emit changed(); return; }
    m_busy = true; m_status = QStringLiteral("正在重新核验并请求安全弹出…"); emit changed(); run(this, id);
}
void UsbManager::finish(const QVariantList &devices, const QString &status, int operationResult)
{
    m_busy = false; m_devices = devices; m_status = status; emit changed();
    if (operationResult != 0) emit operationFinished(operationResult > 0);
}
