#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "include/networkdetails.h"
#include <QTimer>
#include <QVariantMap>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Foundation.h>
#include <vector>

namespace {
QString address(const SOCKET_ADDRESS &value)
{
    if (!value.lpSockaddr) return {};
    wchar_t buffer[INET6_ADDRSTRLEN]{};
    const auto family = value.lpSockaddr->sa_family;
    void *bytes = family == AF_INET ? static_cast<void *>(&reinterpret_cast<sockaddr_in *>(value.lpSockaddr)->sin_addr)
        : family == AF_INET6 ? static_cast<void *>(&reinterpret_cast<sockaddr_in6 *>(value.lpSockaddr)->sin6_addr) : nullptr;
    if (!bytes || !InetNtopW(family, bytes, buffer, INET6_ADDRSTRLEN)) return {};
    QString result = QString::fromWCharArray(buffer);
    if (family == AF_INET6) {
        const auto scope = reinterpret_cast<sockaddr_in6 *>(value.lpSockaddr)->sin6_scope_id;
        if (scope) result += QStringLiteral("%%%1").arg(scope);
    }
    return result;
}
}

NetworkDetails::NetworkDetails(QObject *parent) : QObject(parent)
{
    // A low-frequency read-only snapshot also handles VPN/adapter changes.
    auto *timer = new QTimer(this);
    timer->setInterval(15000);
    connect(timer, &QTimer::timeout, this, &NetworkDetails::refresh);
    timer->start();
    refresh();
}

void NetworkDetails::refresh()
{
    m_adapters.clear();
    m_summary = QStringLiteral("没有默认互联网连接");
    try {
        using namespace winrt::Windows::Networking::Connectivity;
        const auto profile = NetworkInformation::GetInternetConnectionProfile();
        if (profile) {
            const auto level = profile.GetNetworkConnectivityLevel();
            const QString state = level == NetworkConnectivityLevel::InternetAccess ? QStringLiteral("系统报告互联网连接")
                : level == NetworkConnectivityLevel::ConstrainedInternetAccess ? QStringLiteral("受限连接")
                : level == NetworkConnectivityLevel::LocalAccess ? QStringLiteral("仅本地连接") : QStringLiteral("未连接");
            const auto cost = profile.GetConnectionCost();
            const auto type = cost.NetworkCostType();
            const QString metered = type == NetworkCostType::Unrestricted ? QStringLiteral("不按流量计费")
                : type == NetworkCostType::Fixed || type == NetworkCostType::Variable ? QStringLiteral("按流量计费") : QStringLiteral("计费状态未知");
            m_summary = QString::fromStdWString(profile.ProfileName().c_str()) + " · " + state + " · " + metered;
            if (cost.Roaming()) m_summary += QStringLiteral(" · 漫游");
            if (cost.OverDataLimit()) m_summary += QStringLiteral(" · 超出流量限制");
        }
    } catch (...) {
        m_summary = QStringLiteral("Windows 连接概况不可用；以下为适配器信息");
    }
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buffer(size);
    ULONG result = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(size);
        result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_GATEWAYS,
            nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);
        if (size > 4 * 1024 * 1024) break;
    }
    if (result == NO_ERROR) {
        for (auto *adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()); adapter; adapter = adapter->Next) {
            if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->OperStatus != IfOperStatusUp) continue;
            QStringList ips, gateways, dns;
            for (auto *ip = adapter->FirstUnicastAddress; ip; ip = ip->Next) {
                const auto value = address(ip->Address);
                if (!value.isEmpty()) ips.append(value + QStringLiteral("/%1").arg(ip->OnLinkPrefixLength));
            }
            for (auto *gateway = adapter->FirstGatewayAddress; gateway; gateway = gateway->Next) {
                const auto value = address(gateway->Address);
                if (!value.isEmpty()) gateways.append(value);
            }
            for (auto *server = adapter->FirstDnsServerAddress; server; server = server->Next) {
                const auto value = address(server->Address);
                if (!value.isEmpty()) dns.append(value);
            }
            if (ips.isEmpty()) continue;
            m_adapters.append(QVariantMap{
                {"name", QString::fromWCharArray(adapter->FriendlyName)},
                {"description", QString::fromWCharArray(adapter->Description)},
                {"addresses", ips.join('\n')}, {"gateways", gateways.join(" · ")},
                {"dns", dns.join(" · ")}, {"mtu", static_cast<uint>(adapter->Mtu)},
                {"speed", QStringLiteral("↓ %1 / ↑ %2 Mbps").arg(adapter->ReceiveLinkSpeed / 1000000).arg(adapter->TransmitLinkSpeed / 1000000)}
            });
        }
    } else if (result != ERROR_NO_DATA) {
        m_summary += QStringLiteral(" · 适配器读取失败（%1）").arg(result);
    }
    emit changed();
}
