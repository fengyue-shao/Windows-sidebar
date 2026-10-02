#include "include/wifimanager.h"

#include <QTimer>
#include <QVariantMap>
#include <Windows.h>
#include <wlanapi.h>

namespace {
class WlanHandle
{
public:
    WlanHandle() { DWORD version = 0; m_result = WlanOpenHandle(2, nullptr, &version, &m_handle); }
    ~WlanHandle() { if (m_handle) WlanCloseHandle(m_handle, nullptr); }
    HANDLE get() const { return m_handle; }
    DWORD result() const { return m_result; }
private:
    HANDLE m_handle = nullptr;
    DWORD m_result = ERROR_SUCCESS;
};

QString ssidText(const DOT11_SSID &ssid)
{
    return QString::fromUtf8(reinterpret_cast<const char *>(ssid.ucSSID),
                             static_cast<qsizetype>(ssid.uSSIDLength));
}

QString xmlEscape(QString value)
{
    return value.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;")
                .replace('"', "&quot;").replace('\'', "&apos;");
}
}

WifiManager::WifiManager(QObject *parent) : QObject(parent)
{
    QTimer::singleShot(0, this, &WifiManager::scan);
}
WifiManager::~WifiManager() { closeScan(); }
void WifiManager::closeScan()
{
    if (m_scanHandle) {
        WlanRegisterNotification(m_scanHandle, WLAN_NOTIFICATION_SOURCE_NONE, TRUE, nullptr, nullptr, nullptr, nullptr);
        WlanCloseHandle(m_scanHandle, nullptr);
        m_scanHandle = nullptr;
    }
    m_pendingAdapters.clear();
}
void WINAPI WifiManager::scanNotification(PWLAN_NOTIFICATION_DATA data, PVOID context)
{
    if (!data || data->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM) return;
    if (data->NotificationCode != wlan_notification_acm_scan_complete && data->NotificationCode != wlan_notification_acm_scan_fail) return;
    const auto scan = *static_cast<ScanContext *>(context);
    auto *owner = scan.owner;
    const QByteArray adapter(reinterpret_cast<const char *>(&data->InterfaceGuid), sizeof(GUID));
    const bool failed = data->NotificationCode == wlan_notification_acm_scan_fail;
    // Do not close/unregister a WLAN handle from its notification callback.
    QMetaObject::invokeMethod(owner, [owner, generation = scan.generation, adapter, failed] { owner->scanFinished(generation, adapter, failed); }, Qt::QueuedConnection);
}
void WifiManager::scanFinished(quint64 generation, const QByteArray &adapter, bool failed)
{
    if (generation != m_scanGeneration || !m_scanning || !m_pendingAdapters.remove(adapter)) return;
    m_scanFailed |= failed;
    if (!m_pendingAdapters.isEmpty()) return;
    const bool errors = m_scanFailed;
    closeScan(); loadNetworks();
    if (errors) setStatus(QStringLiteral("部分适配器扫描失败，显示系统缓存网络"));
}

void WifiManager::setStatus(const QString &text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusChanged();
}

void WifiManager::scan()
{
    if (m_scanning)
        return;
    m_scanning = true;
    emit scanningChanged();
    setStatus(QStringLiteral("正在扫描附近网络…"));

    closeScan();
    m_scanFailed = false;
    const auto generation = ++m_scanGeneration;
    m_scanContext = {this, generation};
    DWORD version = 0;
    DWORD error = WlanOpenHandle(2, nullptr, &version, &m_scanHandle);
    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (error == ERROR_SUCCESS)
        error = WlanRegisterNotification(m_scanHandle, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, &WifiManager::scanNotification, &m_scanContext, nullptr, nullptr);
    if (error == ERROR_SUCCESS) error = WlanEnumInterfaces(m_scanHandle, nullptr, &interfaces);
    if (error == ERROR_SUCCESS) {
        for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i) {
            const auto &guid = interfaces->InterfaceInfo[i].InterfaceGuid;
            const auto scanError = WlanScan(m_scanHandle, &guid, nullptr, nullptr, nullptr);
            if (scanError == ERROR_SUCCESS)
                m_pendingAdapters.insert(QByteArray(reinterpret_cast<const char *>(&guid), sizeof(GUID)));
            else { m_scanFailed = true; error = scanError; }
        }
    }
    if (interfaces) WlanFreeMemory(interfaces);
    if (m_pendingAdapters.isEmpty()) {
        closeScan(); loadNetworks();
        setStatus(QStringLiteral("无法启动 Wi-Fi 扫描（错误 %1），显示系统缓存").arg(error)); return;
    }
    QTimer::singleShot(4500, this, [this, generation] {
        if (generation != m_scanGeneration || !m_scanning) return;
        closeScan(); loadNetworks();
        setStatus(QStringLiteral("Wi-Fi 扫描超时，显示系统缓存；可稍后刷新"));
    });
}

void WifiManager::loadNetworks()
{
    QVariantList result;
    WlanHandle wlan;
    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (wlan.result() != ERROR_SUCCESS
        || WlanEnumInterfaces(wlan.get(), nullptr, &interfaces) != ERROR_SUCCESS) {
        setStatus(QStringLiteral("无法访问 Windows WLAN 服务"));
    } else {
        for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i) {
            const auto &adapter = interfaces->InterfaceInfo[i];
            PWLAN_AVAILABLE_NETWORK_LIST list = nullptr;
            if (WlanGetAvailableNetworkList(wlan.get(), &adapter.InterfaceGuid,
                    WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
                    nullptr, &list) != ERROR_SUCCESS)
                continue;

            for (DWORD n = 0; n < list->dwNumberOfItems; ++n) {
                const auto &network = list->Network[n];
                const QString ssid = ssidText(network.dot11Ssid);
                if (ssid.isEmpty())
                    continue;
                bool duplicate = false;
                for (const auto &existing : result) {
                    if (existing.toMap().value(QStringLiteral("ssid")).toString() == ssid) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate)
                    continue;
                QVariantMap item;
                item.insert(QStringLiteral("ssid"), ssid);
                item.insert(QStringLiteral("interfaceId"), QByteArray(reinterpret_cast<const char *>(&adapter.InterfaceGuid), sizeof(GUID)));
                item.insert(QStringLiteral("profileName"), QString::fromWCharArray(network.strProfileName));
                item.insert(QStringLiteral("strength"), static_cast<int>(network.wlanSignalQuality));
                item.insert(QStringLiteral("secure"), network.bSecurityEnabled != FALSE);
                item.insert(QStringLiteral("connected"),
                            (network.dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) != 0);
                result.append(item);
            }
            WlanFreeMemory(list);
        }
        WlanFreeMemory(interfaces);
        std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
            const auto left = a.toMap();
            const auto right = b.toMap();
            if (left.value(QStringLiteral("connected")).toBool() != right.value(QStringLiteral("connected")).toBool())
                return left.value(QStringLiteral("connected")).toBool();
            return left.value(QStringLiteral("strength")).toInt() > right.value(QStringLiteral("strength")).toInt();
        });
        setStatus(result.isEmpty() ? QStringLiteral("没有发现 Wi-Fi 网络")
                                   : QStringLiteral("发现 %1 个 Wi-Fi 网络").arg(result.size()));
    }
    m_networks = result;
    m_scanning = false;
    emit networksChanged();
    emit scanningChanged();
}

void WifiManager::connectNetwork(const QString &ssid, const QString &profileName,
                                 const QString &password, bool secure)
{
    WlanHandle wlan;
    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (wlan.result() != ERROR_SUCCESS
        || WlanEnumInterfaces(wlan.get(), nullptr, &interfaces) != ERROR_SUCCESS
        || interfaces->dwNumberOfItems == 0) {
        setStatus(QStringLiteral("没有可用的 Wi-Fi 适配器"));
        if (interfaces) WlanFreeMemory(interfaces);
        return;
    }

    QString targetProfile = profileName;
    GUID interfaceGuid{};
    bool found = false;
    for (const auto &item : m_networks) {
        const auto network = item.toMap();
        if (network.value("ssid").toString() != ssid || network.value("profileName").toString() != profileName) continue;
        const auto id = network.value("interfaceId").toByteArray();
        if (id.size() != sizeof(GUID)) continue;
        for (DWORD index = 0; index < interfaces->dwNumberOfItems; ++index)
            if (memcmp(id.constData(), &interfaces->InterfaceInfo[index].InterfaceGuid, sizeof(GUID)) == 0) {
                interfaceGuid = interfaces->InterfaceInfo[index].InterfaceGuid; found = true; break;
            }
        if (found) break;
    }
    if (!found) { WlanFreeMemory(interfaces); setStatus(QStringLiteral("网络适配器已改变，请刷新后连接")); return; }
    if (targetProfile.isEmpty()) {
        if (secure && password.size() < 8) {
            setStatus(QStringLiteral("WPA2 密码至少需要 8 个字符"));
            WlanFreeMemory(interfaces);
            return;
        }
        targetProfile = ssid;
        const QString escapedSsid = xmlEscape(ssid);
        const QString security = secure
            ? QStringLiteral("<authEncryption><authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX></authEncryption><sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>%1</keyMaterial></sharedKey>").arg(xmlEscape(password))
            : QStringLiteral("<authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption>");
        const QString profileXml = QStringLiteral(
            "<?xml version=\"1.0\"?><WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\"><name>%1</name><SSIDConfig><SSID><name>%1</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>auto</connectionMode><MSM><security>%2</security></MSM></WLANProfile>")
            .arg(escapedSsid, security);
        DWORD reason = 0;
        const DWORD profileResult = WlanSetProfile(wlan.get(), &interfaceGuid, 0,
            reinterpret_cast<LPCWSTR>(profileXml.utf16()), nullptr, TRUE, nullptr, &reason);
        if (profileResult != ERROR_SUCCESS) {
            setStatus(QStringLiteral("无法创建网络配置，请检查密码或安全类型"));
            WlanFreeMemory(interfaces);
            return;
        }
    }

    WLAN_CONNECTION_PARAMETERS parameters{};
    parameters.wlanConnectionMode = wlan_connection_mode_profile;
    parameters.strProfile = reinterpret_cast<LPCWSTR>(targetProfile.utf16());
    parameters.dot11BssType = dot11_BSS_type_any;
    parameters.dwFlags = 0;
    const DWORD connectResult = WlanConnect(wlan.get(), &interfaceGuid, &parameters, nullptr);
    WlanFreeMemory(interfaces);
    setStatus(connectResult == ERROR_SUCCESS
        ? QStringLiteral("正在连接 %1…").arg(ssid)
        : QStringLiteral("连接请求失败（错误 %1）").arg(connectResult));
    if (connectResult == ERROR_SUCCESS)
        QTimer::singleShot(3500, this, [this] { if (!m_scanning) loadNetworks(); });
}
