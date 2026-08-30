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

    WlanHandle wlan;
    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (wlan.result() == ERROR_SUCCESS
        && WlanEnumInterfaces(wlan.get(), nullptr, &interfaces) == ERROR_SUCCESS) {
        for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i)
            WlanScan(wlan.get(), &interfaces->InterfaceInfo[i].InterfaceGuid, nullptr, nullptr, nullptr);
        WlanFreeMemory(interfaces);
    }

    // WlanScan completes asynchronously; read the refreshed cache shortly after.
    QTimer::singleShot(1800, this, &WifiManager::loadNetworks);
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
    const GUID interfaceGuid = interfaces->InterfaceInfo[0].InterfaceGuid;
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
        QTimer::singleShot(3500, this, &WifiManager::loadNetworks);
}
