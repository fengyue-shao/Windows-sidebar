#include "include/hotspotmanager.h"
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <windows.h>
#include <appmodel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Networking.NetworkOperators.h>

namespace {
using namespace winrt::Windows::Networking::NetworkOperators;
NetworkOperatorTetheringManager manager()
{
    const auto profile = winrt::Windows::Networking::Connectivity::NetworkInformation::GetInternetConnectionProfile();
    if (!profile || NetworkOperatorTetheringManager::GetTetheringCapabilityFromConnectionProfile(profile) != TetheringCapability::Enabled)
        return nullptr;
    return NetworkOperatorTetheringManager::CreateFromConnectionProfile(profile);
}

winrt::fire_and_forget operate(QPointer<HotspotManager> owner, bool configure, bool active, QString ssid, QString password)
{
    QString message;
    try {
        const auto hotspot = manager();
        if (!hotspot) {
            message = QStringLiteral("当前连接不支持共享，请检查网络、硬件和系统策略");
        } else if (configure) {
            if (hotspot.TetheringOperationalState() != TetheringOperationalState::Off) {
                message = QStringLiteral("请先关闭热点，再更改名称和密码");
            } else {
                auto config = hotspot.GetCurrentAccessPointConfiguration();
                config.Ssid(ssid.toStdWString());
                config.Passphrase(password.toStdWString());
                password.fill(QChar(0)); password.clear();
                co_await hotspot.ConfigureAccessPointAsync(config);
                message = QStringLiteral("热点配置已保存到 Windows");
            }
        } else {
            const auto result = active ? co_await hotspot.StartTetheringAsync() : co_await hotspot.StopTetheringAsync();
            const auto status = result.Status();
            if (status == TetheringOperationStatus::Success || (active && status == TetheringOperationStatus::AlreadyOn))
                message = active ? QStringLiteral("开启请求已完成") : QStringLiteral("关闭请求已完成");
            else if (status == TetheringOperationStatus::WiFiDeviceOff)
                message = QStringLiteral("Wi-Fi 已关闭，请先手动开启 Wi-Fi");
            else if (status == TetheringOperationStatus::NetworkLimitedConnectivity)
                message = QStringLiteral("上游网络受限，无法开启热点");
            else if (status == TetheringOperationStatus::OperationInProgress)
                message = QStringLiteral("Windows 正在处理另一个热点操作，请稍后刷新");
            else message = QStringLiteral("热点操作失败（状态 %1）").arg(static_cast<int>(status));
        }
    } catch (const winrt::hresult_error &error) {
        message = QStringLiteral("热点操作失败（0x%1），请检查权限和系统策略")
            .arg(static_cast<quint32>(error.code().value), 8, 16, QLatin1Char('0'));
    } catch (...) { message = QStringLiteral("热点操作失败"); }
    password.fill(QChar(0)); password.clear();
    QMetaObject::invokeMethod(qGuiApp, [owner, message] { if (owner) owner->finished(message); }, Qt::QueuedConnection);
}
}

HotspotManager::HotspotManager(QObject *parent) : QObject(parent)
{
    auto *timer = new QTimer(this);
    timer->setInterval(10000);
    connect(timer, &QTimer::timeout, this, &HotspotManager::refresh);
    timer->start(); refresh();
}

void HotspotManager::refresh()
{
    if (m_busy) return;
    m_supported = false; m_active = false; m_ssid.clear();
    UINT32 length = 0;
    if (GetCurrentPackageFullName(&length, nullptr) == APPMODEL_ERROR_NO_PACKAGE) {
        m_status = QStringLiteral("需要 MSIX 版本及 Wi-Fi 控制权限"); emit changed(); return;
    }
    try {
        const auto hotspot = manager();
        if (!hotspot) m_status = QStringLiteral("当前网络、硬件或系统策略不支持移动热点");
        else {
            const auto state = hotspot.TetheringOperationalState();
            m_active = state == TetheringOperationalState::On;
            m_supported = state == TetheringOperationalState::On || state == TetheringOperationalState::Off;
            m_ssid = QString::fromStdWString(hotspot.GetCurrentAccessPointConfiguration().Ssid().c_str());
            m_status = m_active ? QStringLiteral("已开启 · %1/%2 台设备").arg(hotspot.ClientCount()).arg(hotspot.MaxClientCount())
                : state == TetheringOperationalState::Off ? QStringLiteral("已关闭") : QStringLiteral("Windows 正在切换热点状态");
        }
    } catch (const winrt::hresult_error &error) {
        m_status = QStringLiteral("无法访问热点（0x%1）").arg(static_cast<quint32>(error.code().value), 8, 16, QLatin1Char('0'));
    } catch (...) { m_status = QStringLiteral("无法读取热点状态"); }
    if (!m_result.isEmpty()) m_status += " · " + m_result;
    emit changed();
}

bool HotspotManager::validConfiguration(const QString &ssid, const QString &password)
{
    if (ssid.trimmed().isEmpty() || ssid.toUtf8().size() > 32 || ssid.contains(QChar(0)) || ssid.contains('\n') || ssid.contains('\r')) return false;
    if (password.size() < 8 || password.size() > 63) return false;
    for (const QChar c : password) if (c.unicode() < 32 || c.unicode() > 126) return false;
    return true;
}

void HotspotManager::setActive(bool active)
{
    if (m_busy) return;
    refresh(); // Recheck support and state immediately before any operation.
    if (!m_supported || m_active == active) return;
    m_busy = true; m_result.clear(); m_status = QStringLiteral("正在提交热点操作…"); emit changed();
    operate(this, false, active, {}, {});
}

void HotspotManager::configure(const QString &ssid, const QString &password)
{
    if (m_busy) return;
    if (!validConfiguration(ssid, password)) {
        m_result = QStringLiteral("名称限 1–32 UTF-8 字节；密码需 8–63 位可打印英文字符"); refresh(); return;
    }
    refresh();
    if (!m_supported || m_active) return;
    m_busy = true; m_result.clear(); m_status = QStringLiteral("正在保存热点配置…"); emit changed();
    operate(this, true, false, ssid, password);
}

void HotspotManager::finished(const QString &message)
{
    m_busy = false; m_result = message; refresh();
}
