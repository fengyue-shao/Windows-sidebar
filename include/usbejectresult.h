#pragma once
#include <QString>
#include <windows.h>
#include <cfgmgr32.h>

namespace SidebarUsb {
inline QString vetoReason(PNP_VETO_TYPE veto)
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
    case PNP_VetoLegacyDevice: return QStringLiteral("设备不支持此移除操作");
    case PNP_VetoIllegalDeviceRequest: return QStringLiteral("设备不支持此请求");
    case PNP_VetoInsufficientPower: return QStringLiteral("设备电源条件不足");
    case PNP_VetoLegacyDriver: return QStringLiteral("驱动程序不支持此操作");
    case PNP_VetoAlreadyRemoved: return QStringLiteral("设备已经移除，请刷新列表");
    default: return QStringLiteral("系统或设备拒绝移除（%1）").arg(static_cast<int>(veto));
    }
}
inline QString ejectMessage(CONFIGRET result, PNP_VETO_TYPE veto, const QString &relatedObject)
{
    if (result == CR_SUCCESS) return QStringLiteral("Windows 已接受安全弹出请求，请确认设备消失后再拔出");
    return QStringLiteral("%1（代码 %2）%3；未强制移除，请处理原因后刷新重试")
        .arg(vetoReason(veto)).arg(result)
        .arg(relatedObject.isEmpty() ? QString() : QStringLiteral(" · 关联对象：") + relatedObject);
}
}
