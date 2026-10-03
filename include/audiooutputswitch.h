#pragma once
#include <QString>
#include <QStringList>
#include <mmdeviceapi.h>

namespace SidebarAudio {
struct EndpointRead { HRESULT result; QString deviceId; };
struct OutputSwitchResult { bool confirmed = false; int matchingRoles = 0; QString message; };
inline QString errorCode(HRESULT result)
{
    return QStringLiteral("0x%1").arg(QString::number(quint32(result), 16)
        .rightJustified(8, QLatin1Char('0')).toUpper());
}
inline QString roleName(ERole role)
{
    switch (role) {
    case eConsole: return QStringLiteral("系统声音");
    case eMultimedia: return QStringLiteral("媒体");
    default: return QStringLiteral("通话");
    }
}
// Use the same decision path in production and tests, without test system writes.
template<typename SetDefault, typename ReadDefault>
OutputSwitchResult switchOutput(const QString &deviceId, SetDefault setDefault, ReadDefault readDefault)
{
    if (deviceId.isEmpty()) return {false, 0, QStringLiteral("请选择可用的输出设备")};
    QStringList issues;
    constexpr ERole roles[] = {eConsole, eMultimedia, eCommunications};
    for (const auto role : roles) {
        const HRESULT result = setDefault(role);
        if (FAILED(result)) issues.append(roleName(role) + QStringLiteral("设置失败 ") + errorCode(result));
    }
    int matching = 0;
    for (const auto role : roles) {
        const EndpointRead current = readDefault(role);
        if (FAILED(current.result))
            issues.append(roleName(role) + QStringLiteral("状态读取失败 ") + errorCode(current.result));
        else if (current.deviceId != deviceId)
            issues.append(roleName(role) + QStringLiteral("默认设备尚未匹配"));
        else ++matching;
    }
    if (issues.isEmpty()) return {true, matching, QStringLiteral("输出设备切换已确认")};
    return {false, matching, QStringLiteral("未完整确认输出切换（%1/3）：%2")
        .arg(matching).arg(issues.join(QStringLiteral("；")))};
}
}
