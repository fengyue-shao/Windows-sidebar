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
    HANDLE release() { const auto handle = m_handle; m_handle = nullptr; return handle; }
private:
    HANDLE m_handle = nullptr;
    DWORD m_result = ERROR_SUCCESS;
};

QString ssidText(const DOT11_SSID &ssid)
{
    if (ssid.uSSIDLength > DOT11_SSID_MAX_LENGTH) return {};
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
WifiManager::~WifiManager() { closeConnection(); closeScan(); }
void WifiManager::closeConnection()
{
    if (m_connectionHandle) {
        // Always outside the WLAN callback: unregister waits for callbacks to finish.
        WlanRegisterNotification(m_connectionHandle, WLAN_NOTIFICATION_SOURCE_NONE, TRUE, nullptr, nullptr, nullptr, nullptr);
        WlanCloseHandle(m_connectionHandle, nullptr);
        m_connectionHandle = nullptr;
    }
    m_connection.finish();
    m_disconnecting = false;
    m_forgetAfterDisconnect.clear();
}
void WINAPI WifiManager::connectionNotification(PWLAN_NOTIFICATION_DATA data, PVOID context)
{
    if (!data || data->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM || !context) return;
    if (data->NotificationCode != wlan_notification_acm_connection_complete && data->NotificationCode != wlan_notification_acm_connection_attempt_fail && data->NotificationCode != wlan_notification_acm_disconnected) return;
    // The profile XML tail is variable-length. Only copy fixed fields while pData is valid.
    if (!data->pData || data->dwDataSize < offsetof(WLAN_CONNECTION_NOTIFICATION_DATA, strProfileXml)) return;
    const auto *notice = static_cast<const WLAN_CONNECTION_NOTIFICATION_DATA *>(data->pData);
    if (notice->dot11Ssid.uSSIDLength > DOT11_SSID_MAX_LENGTH) return;
    const auto ctx = *static_cast<ScanContext *>(context);
    const QByteArray adapter(reinterpret_cast<const char *>(&data->InterfaceGuid), sizeof(GUID));
    const QByteArray ssid(reinterpret_cast<const char *>(notice->dot11Ssid.ucSSID), notice->dot11Ssid.uSSIDLength);
    size_t length = 0;
    while (length < WLAN_MAX_NAME_LENGTH && notice->strProfileName[length]) ++length;
    if (length == WLAN_MAX_NAME_LENGTH) return;
    const auto profile = QString::fromWCharArray(notice->strProfileName, static_cast<qsizetype>(length));
    const auto code = data->NotificationCode, reason = notice->wlanReasonCode;
    QMetaObject::invokeMethod(ctx.owner, [owner=ctx.owner, token=ctx.generation, adapter, ssid, profile, code, reason] {
        owner->connectionEvent(token, adapter, ssid, profile, code, reason);
    }, Qt::QueuedConnection);
}
bool WifiManager::connectionConfirmed() const
{
    if (!m_connectionHandle || m_connection.adapter.size() != sizeof(GUID)) return false;
    GUID adapter{}; memcpy(&adapter, m_connection.adapter.constData(), sizeof(adapter));
    DWORD size = 0; PVOID value = nullptr;
    const DWORD result = WlanQueryInterface(m_connectionHandle, &adapter, wlan_intf_opcode_current_connection, nullptr, &size, &value, nullptr);
    bool confirmed = false;
    if (result == ERROR_SUCCESS && value && size >= sizeof(WLAN_CONNECTION_ATTRIBUTES)) {
        const auto *current = static_cast<const WLAN_CONNECTION_ATTRIBUTES *>(value);
        const auto &ssid = current->wlanAssociationAttributes.dot11Ssid;
        confirmed = current->isState == wlan_interface_state_connected && ssid.uSSIDLength <= DOT11_SSID_MAX_LENGTH
            && QByteArray(reinterpret_cast<const char *>(ssid.ucSSID), ssid.uSSIDLength) == m_connection.ssid
            && QString::fromWCharArray(current->strProfileName) == m_connection.profile;
    }
    if (value) WlanFreeMemory(value);
    return confirmed;
}
void WifiManager::finishConnection(const QString &status)
{
    closeConnection();
    emit connectingChanged();
    if (!m_scanning) loadNetworks();
    setStatus(status);
}
void WifiManager::connectionEvent(quint64 generation, const QByteArray &adapter, const QByteArray &ssid, const QString &profile, DWORD code, DWORD reason)
{
    if (m_disconnecting && connecting() && generation == m_connection.generation && adapter == m_connection.adapter) {
        if (code == wlan_notification_acm_disconnected) finishDisconnection();
        return;
    }
    const auto result = m_connection.event(generation, adapter, ssid, profile, code, reason);
    if (result == SidebarWifi::EventResult::Ignore) return;
    wchar_t reasonText[512]{};
    WlanReasonCodeToString(reason, 512, reasonText, nullptr);
    const auto details = QString::fromWCharArray(reasonText).trimmed();
    if (result == SidebarWifi::EventResult::AttemptFailed) {
        setStatus(QStringLiteral("本次尝试未成功，Windows 可能继续尝试（原因 %1）：%2").arg(reason).arg(details));
    } else if (result == SidebarWifi::EventResult::CompleteFailure) {
        finishConnection(QStringLiteral("Wi-Fi 连接失败（原因 %1）：%2").arg(reason).arg(details));
    } else {
        finishConnection(connectionConfirmed() ? QStringLiteral("已确认连接到 %1").arg(QString::fromUtf8(m_connection.ssid))
                                               : QStringLiteral("Windows 报告连接完成，但当前连接尚未核验；请刷新"));
    }
}
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
    if (m_scanning || connecting())
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
    QVariantList adapters;
    WlanHandle wlan;
    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (wlan.result() != ERROR_SUCCESS
        || WlanEnumInterfaces(wlan.get(), nullptr, &interfaces) != ERROR_SUCCESS) {
        setStatus(QStringLiteral("无法访问 Windows WLAN 服务"));
    } else {
        for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i) {
            const auto &adapter = interfaces->InterfaceInfo[i];
            adapters.append(QVariantMap{{"name", QString::fromWCharArray(adapter.strInterfaceDescription)},
                {"interfaceId", QByteArray(reinterpret_cast<const char *>(&adapter.InterfaceGuid), sizeof(GUID))}});
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
                    if (existing.toMap().value(QStringLiteral("ssidBytes")).toByteArray() == QByteArray(reinterpret_cast<const char *>(network.dot11Ssid.ucSSID), network.dot11Ssid.uSSIDLength)
                        && existing.toMap().value("interfaceId").toByteArray() == QByteArray(reinterpret_cast<const char *>(&adapter.InterfaceGuid), sizeof(GUID))
                        && existing.toMap().value("profileName").toString() == QString::fromWCharArray(network.strProfileName)) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate)
                    continue;
                QVariantMap item;
                item.insert(QStringLiteral("ssid"), ssid);
                item.insert(QStringLiteral("ssidBytes"), QByteArray(reinterpret_cast<const char *>(network.dot11Ssid.ucSSID), network.dot11Ssid.uSSIDLength));
                item.insert(QStringLiteral("auth"), static_cast<int>(network.dot11DefaultAuthAlgorithm));
                item.insert(QStringLiteral("cipher"), static_cast<int>(network.dot11DefaultCipherAlgorithm));
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
    m_adapters = adapters;
    m_scanning = false;
    emit networksChanged();
    emit scanningChanged();
}

void WifiManager::connectNetwork(const QString &ssid, const QString &profileName,
                                 const QString &password, bool secure, const QByteArray &interfaceId)
{
    if (connecting() || m_scanning) return;
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
    QByteArray rawSsid;
    QVariantMap selected;
    for (const auto &item : m_networks) {
        const auto network = item.toMap();
        if (network.value("ssid").toString() != ssid || network.value("profileName").toString() != profileName) continue;
        const auto id = network.value("interfaceId").toByteArray();
        if (!interfaceId.isEmpty() && id != interfaceId) continue;
        if (id.size() != sizeof(GUID)) continue;
        for (DWORD index = 0; index < interfaces->dwNumberOfItems; ++index)
            if (memcmp(id.constData(), &interfaces->InterfaceInfo[index].InterfaceGuid, sizeof(GUID)) == 0) {
                interfaceGuid = interfaces->InterfaceInfo[index].InterfaceGuid; found = true; selected = network; break;
            }
        if (found) break;
    }
    if (!found) { WlanFreeMemory(interfaces); setStatus(QStringLiteral("网络适配器已改变，请刷新后连接")); return; }
    rawSsid = selected.value("ssidBytes").toByteArray();
    if (rawSsid.isEmpty() || rawSsid.size() > DOT11_SSID_MAX_LENGTH) {
        WlanFreeMemory(interfaces); setStatus(QStringLiteral("网络标识无效，请刷新")); return;
    }
    if (targetProfile.isEmpty()) {
        const int auth = selected.value("auth").toInt(), cipher = selected.value("cipher").toInt();
        if (!SidebarWifi::supportedNewProfile(auth, cipher, secure)) {
            WlanFreeMemory(interfaces); setStatus(QStringLiteral("该网络的安全类型不支持在应用内新建配置，请使用 Windows Wi-Fi 设置")); return;
        }
        if (secure && (password.size() < 8 || password.size() > 63)) {
            setStatus(QStringLiteral("此入口支持 8–63 字符的 WPA2 密码；其他密钥格式请使用系统设置"));
            WlanFreeMemory(interfaces);
            return;
        }
        targetProfile = ssid;
        const QString escapedSsid = xmlEscape(ssid);
        const QString security = secure
            ? QStringLiteral("<authEncryption><authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX></authEncryption><sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>%1</keyMaterial></sharedKey>").arg(xmlEscape(password))
            : QStringLiteral("<authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption>");
        const QString profileXml = QStringLiteral(
            "<?xml version=\"1.0\"?><WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\"><name>%1</name><SSIDConfig><SSID><hex>%3</hex></SSID>%4</SSIDConfig><connectionType>ESS</connectionType><connectionMode>auto</connectionMode><MSM><security>%2</security></MSM></WLANProfile>")
            .arg(escapedSsid, security, QString::fromLatin1(rawSsid.toHex()), selected.value("hidden").toBool() ? QStringLiteral("<nonBroadcast>true</nonBroadcast>") : QString{});
        DWORD reason = 0;
        const DWORD profileResult = WlanSetProfile(wlan.get(), &interfaceGuid, 0,
            reinterpret_cast<LPCWSTR>(profileXml.utf16()), nullptr, FALSE, nullptr, &reason);
        if (profileResult != ERROR_SUCCESS) {
            setStatus(QStringLiteral("无法创建网络配置（错误 %1，原因 %2）；不会覆盖同名配置，请使用系统设置检查").arg(profileResult).arg(reason));
            WlanFreeMemory(interfaces);
            return;
        }
    }

    WLAN_CONNECTION_PARAMETERS parameters{};
    parameters.wlanConnectionMode = wlan_connection_mode_profile;
    parameters.strProfile = reinterpret_cast<LPCWSTR>(targetProfile.utf16());
    parameters.dot11BssType = dot11_BSS_type_any;
    parameters.dwFlags = 0;
    m_connection.begin(QByteArray(reinterpret_cast<const char *>(&interfaceGuid), sizeof(interfaceGuid)), rawSsid, targetProfile);
    m_connectionContext = {this, m_connection.generation};
    m_connectionHandle = wlan.release();
    const DWORD registration = WlanRegisterNotification(m_connectionHandle, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, &WifiManager::connectionNotification, &m_connectionContext, nullptr, nullptr);
    if (registration != ERROR_SUCCESS) {
        WlanFreeMemory(interfaces);
        finishConnection(QStringLiteral("无法监听 Wi-Fi 连接结果（错误 %1），未提交连接请求").arg(registration));
        return;
    }
    emit connectingChanged();
    const DWORD connectResult = WlanConnect(m_connectionHandle, &interfaceGuid, &parameters, nullptr);
    WlanFreeMemory(interfaces);
    setStatus(connectResult == ERROR_SUCCESS
        ? QStringLiteral("正在连接 %1…").arg(ssid)
        : QStringLiteral("连接请求失败（错误 %1）").arg(connectResult));
    if (connectResult != ERROR_SUCCESS) { finishConnection(m_statusText); return; }
    const auto generation = m_connection.generation;
    QTimer::singleShot(30000, this, [this, generation] {
        if (!connecting() || generation != m_connection.generation) return;
        const bool confirmed = connectionConfirmed();
        finishConnection(confirmed ? QStringLiteral("已核验 Wi-Fi 连接（未收到完成事件）")
                                   : QStringLiteral("连接等待超时；仅停止本应用等待，Windows 可能继续连接，请刷新或在系统设置中检查"));
    });
}

void WifiManager::connectHiddenNetwork(const QString &ssid, const QString &password, bool secure, const QByteArray &adapter)
{
    if (connecting() || m_scanning) return;
    const auto raw = ssid.toUtf8();
    if (raw.isEmpty() || raw.size() > DOT11_SSID_MAX_LENGTH || adapter.size() != sizeof(GUID)) {
        setStatus(QStringLiteral("请输入 1–32 字节的网络名称并选择适配器")); return;
    }
    for (const auto &item : m_networks) {
        const auto network = item.toMap();
        if (network.value("ssidBytes").toByteArray() == raw && network.value("interfaceId").toByteArray() == adapter) {
            setStatus(QStringLiteral("该网络已在列表中，请从列表连接；不会替换已有配置")); return;
        }
    }
    m_networks.append(QVariantMap{{"ssid", ssid}, {"ssidBytes", raw}, {"profileName", ""}, {"interfaceId", adapter},
        {"secure", secure}, {"connected", false}, {"strength", 0}, {"hidden", true},
        {"auth", secure ? DOT11_AUTH_ALGO_RSNA_PSK : DOT11_AUTH_ALGO_80211_OPEN},
        {"cipher", secure ? DOT11_CIPHER_ALGO_CCMP : DOT11_CIPHER_ALGO_NONE}});
    emit networksChanged();
    connectNetwork(ssid, "", password, secure, adapter);
}

bool WifiManager::disconnectedConfirmed() const
{
    if (!m_connectionHandle || m_connection.adapter.size() != sizeof(GUID)) return false;
    GUID adapter{}; memcpy(&adapter, m_connection.adapter.constData(), sizeof(adapter));
    DWORD size = 0; PVOID value = nullptr;
    const DWORD result = WlanQueryInterface(m_connectionHandle, &adapter, wlan_intf_opcode_interface_state, nullptr, &size, &value, nullptr);
    const bool disconnected = result == ERROR_SUCCESS && value && size >= sizeof(WLAN_INTERFACE_STATE)
        && *static_cast<WLAN_INTERFACE_STATE *>(value) == wlan_interface_state_disconnected;
    if (value) WlanFreeMemory(value);
    return disconnected;
}
void WifiManager::finishDisconnection()
{
    if (!disconnectedConfirmed()) { finishConnection(QStringLiteral("尚未核验断开状态，未删除网络配置；请刷新")); return; }
    QString message = QStringLiteral("已确认断开 Wi-Fi");
    if (!m_forgetAfterDisconnect.isEmpty()) {
        GUID adapter{}; memcpy(&adapter, m_connection.adapter.constData(), sizeof(adapter));
        const DWORD result = WlanDeleteProfile(m_connectionHandle, &adapter, reinterpret_cast<LPCWSTR>(m_forgetAfterDisconnect.utf16()), nullptr);
        message = result == ERROR_SUCCESS ? QStringLiteral("已断开并忘记网络配置")
                                         : QStringLiteral("已断开，但忘记配置失败（错误 %1）").arg(result);
    }
    finishConnection(message);
}
void WifiManager::disconnectNetwork(const QString &ssid, const QString &profile, const QByteArray &adapter)
{
    requestDisconnect(ssid, profile, adapter, false);
}
void WifiManager::forgetNetwork(const QString &ssid, const QString &profile, const QByteArray &adapter, bool confirmDisconnect)
{
    if (!confirmDisconnect) { setStatus(QStringLiteral("忘记配置需二次确认；当前连接将先断开")); return; }
    if (profile.isEmpty()) { setStatus(QStringLiteral("没有已保存的网络配置可忘记")); return; }
    requestDisconnect(ssid, profile, adapter, true);
}
void WifiManager::requestDisconnect(const QString &ssid, const QString &profile, const QByteArray &adapter, bool forget)
{
    if (connecting() || m_scanning) return;
    QVariantMap selected;
    for (const auto &item : m_networks) {
        const auto network = item.toMap();
        if (network.value("interfaceId").toByteArray() == adapter && network.value("ssid").toString() == ssid && network.value("profileName").toString() == profile) { selected = network; break; }
    }
    if (selected.isEmpty() || adapter.size() != sizeof(GUID)) { setStatus(QStringLiteral("网络或适配器已改变，请刷新")); return; }
    WlanHandle wlan;
    if (wlan.result() != ERROR_SUCCESS) { setStatus(QStringLiteral("无法访问 WLAN 服务")); return; }
    GUID guid{}; memcpy(&guid, adapter.constData(), sizeof(guid));
    DWORD size = 0; PVOID value = nullptr;
    const DWORD query = WlanQueryInterface(wlan.get(), &guid, wlan_intf_opcode_interface_state, nullptr, &size, &value, nullptr);
    const bool disconnected = query == ERROR_SUCCESS && value && size >= sizeof(WLAN_INTERFACE_STATE)
        && *static_cast<WLAN_INTERFACE_STATE *>(value) == wlan_interface_state_disconnected;
    const bool connected = query == ERROR_SUCCESS && value && size >= sizeof(WLAN_INTERFACE_STATE)
        && *static_cast<WLAN_INTERFACE_STATE *>(value) == wlan_interface_state_connected;
    if (value) WlanFreeMemory(value);
    if (query != ERROR_SUCCESS) { setStatus(QStringLiteral("无法核验适配器状态（错误 %1），未执行操作").arg(query)); return; }
    if (!disconnected && !connected) { setStatus(QStringLiteral("适配器正在改变连接状态，请稍后刷新；未执行断开或忘记")); return; }
    if (disconnected && !forget) { setStatus(QStringLiteral("Wi-Fi 已断开")); return; }
    bool targetCurrent = false;
    if (connected) {
        value = nullptr; size = 0;
        const DWORD currentResult = WlanQueryInterface(wlan.get(), &guid, wlan_intf_opcode_current_connection, nullptr, &size, &value, nullptr);
        if (currentResult != ERROR_SUCCESS || !value || size < sizeof(WLAN_CONNECTION_ATTRIBUTES)) {
            if (value) WlanFreeMemory(value);
            setStatus(QStringLiteral("无法核验当前配置，未执行断开或忘记")); return;
        }
        const auto *current = static_cast<const WLAN_CONNECTION_ATTRIBUTES *>(value);
        targetCurrent = QString::fromWCharArray(current->strProfileName) == profile;
        WlanFreeMemory(value);
    }
    // An unrelated currently connected profile must not be disconnected.
    const auto action = SidebarWifi::decideAction(forget, true, disconnected, connected, true, targetCurrent);
    if (action == SidebarWifi::Action::Reject) { setStatus(QStringLiteral("当前连接已改变，未断开其他网络；请刷新")); return; }
    m_connection.begin(adapter, selected.value("ssidBytes").toByteArray(), profile);
    m_connectionHandle = wlan.release();
    if (connected && action == SidebarWifi::Action::ForgetOnly) {
        if (forget) {
            const DWORD result = WlanDeleteProfile(m_connectionHandle, &guid, reinterpret_cast<LPCWSTR>(profile.utf16()), nullptr);
            finishConnection(result == ERROR_SUCCESS ? QStringLiteral("已忘记非当前网络配置") : QStringLiteral("忘记失败（错误 %1）").arg(result));
        } else finishConnection(QStringLiteral("当前连接已改变，未断开其他网络；请刷新"));
        return;
    }
    m_disconnecting = true; m_forgetAfterDisconnect = forget ? profile : QString{};
    if (disconnected) { finishDisconnection(); return; }
    m_connectionContext = {this, m_connection.generation};
    DWORD result = WlanRegisterNotification(m_connectionHandle, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, &WifiManager::connectionNotification, &m_connectionContext, nullptr, nullptr);
    if (result == ERROR_SUCCESS) result = WlanDisconnect(m_connectionHandle, &guid, nullptr);
    if (result != ERROR_SUCCESS) { finishConnection(QStringLiteral("断开请求失败（错误 %1），未忘记配置").arg(result)); return; }
    emit connectingChanged(); setStatus(QStringLiteral("正在断开 Wi-Fi…"));
    const auto generation = m_connection.generation;
    QTimer::singleShot(15000, this, [this, generation] {
        if (!connecting() || generation != m_connection.generation || !m_disconnecting) return;
        if (disconnectedConfirmed()) finishDisconnection();
        else finishConnection(QStringLiteral("断开等待超时，未删除配置；系统请求不保证已取消，请刷新"));
    });
}
