#pragma once
#include <QByteArray>
#include <QString>
#include <Windows.h>
#include <wlanapi.h>

namespace SidebarWifi {
enum class Action { Reject, AlreadyDisconnected, Disconnect, ForgetOnly };
inline Action decideAction(bool forget, bool stateKnown, bool disconnected, bool connected, bool currentKnown, bool targetCurrent) {
    if (!stateKnown || (!disconnected && !connected) || (connected && !currentKnown)) return Action::Reject;
    if (disconnected) return forget ? Action::ForgetOnly : Action::AlreadyDisconnected;
    if (!targetCurrent) return forget ? Action::ForgetOnly : Action::Reject;
    return Action::Disconnect;
}
inline bool supportedNewProfile(int auth, int cipher, bool secure) {
    return secure ? auth == DOT11_AUTH_ALGO_RSNA_PSK && cipher == DOT11_CIPHER_ALGO_CCMP
                  : auth == DOT11_AUTH_ALGO_80211_OPEN && cipher == DOT11_CIPHER_ALGO_NONE;
}
enum class EventResult { Ignore, AttemptFailed, CompleteSuccess, CompleteFailure };
struct ConnectionState {
    quint64 generation = 0;
    bool active = false;
    QByteArray adapter, ssid;
    QString profile;
    void begin(const QByteArray &interfaceId, const QByteArray &rawSsid, const QString &profileName) {
        ++generation; active = true; adapter = interfaceId; ssid = rawSsid; profile = profileName;
    }
    EventResult event(quint64 token, const QByteArray &interfaceId, const QByteArray &rawSsid,
                      const QString &profileName, DWORD code, DWORD reason) const {
        if (!active || token != generation || interfaceId != adapter || rawSsid != ssid || profileName != profile)
            return EventResult::Ignore;
        if (code == wlan_notification_acm_connection_attempt_fail) return EventResult::AttemptFailed;
        if (code != wlan_notification_acm_connection_complete) return EventResult::Ignore;
        return reason == WLAN_REASON_CODE_SUCCESS ? EventResult::CompleteSuccess : EventResult::CompleteFailure;
    }
    bool expire(quint64 token) { if (!active || generation != token) return false; active = false; return true; }
    void finish() { active = false; }
};
}
