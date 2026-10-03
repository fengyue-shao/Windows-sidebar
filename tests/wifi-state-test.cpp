#include "include/wificonnectionstate.h"
#include <QCoreApplication>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using namespace SidebarWifi;
    CHECK(decideAction(true, false, false, true, true, true) == Action::Reject);
    CHECK(decideAction(true, true, false, true, false, true) == Action::Reject);
    CHECK(decideAction(true, true, false, false, true, true) == Action::Reject);
    CHECK(decideAction(false, true, false, true, true, false) == Action::Reject);
    CHECK(decideAction(true, true, false, true, true, false) == Action::ForgetOnly);
    CHECK(decideAction(true, true, false, true, true, true) == Action::Disconnect);
    CHECK(decideAction(true, true, true, false, true, false) == Action::ForgetOnly);
    CHECK(decideAction(false, true, true, false, true, false) == Action::AlreadyDisconnected);
    CHECK(supportedNewProfile(DOT11_AUTH_ALGO_RSNA_PSK, DOT11_CIPHER_ALGO_CCMP, true));
    CHECK(!supportedNewProfile(DOT11_AUTH_ALGO_RSNA, DOT11_CIPHER_ALGO_CCMP, true));
    CHECK(!supportedNewProfile(DOT11_AUTH_ALGO_RSNA_PSK, DOT11_CIPHER_ALGO_TKIP, true));
    CHECK(supportedNewProfile(DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_NONE, false));
    CHECK(!supportedNewProfile(DOT11_AUTH_ALGO_80211_OPEN, DOT11_CIPHER_ALGO_WEP, false));
    ConnectionState state;
    const QByteArray adapter(16, 'a'), ssid("test");
    state.begin(adapter, ssid, "profile"); const auto token = state.generation;
    CHECK(state.active);
    CHECK(state.event(token, adapter, ssid, "profile", wlan_notification_acm_connection_attempt_fail, 123) == EventResult::AttemptFailed);
    CHECK(state.active); // Per-SSID attempt failure is not final failure.
    CHECK(state.event(token, adapter, ssid, "profile", wlan_notification_acm_connection_complete, 123) == EventResult::CompleteFailure);
    CHECK(state.event(token, adapter, ssid, "profile", wlan_notification_acm_connection_complete, WLAN_REASON_CODE_SUCCESS) == EventResult::CompleteSuccess);
    CHECK(state.event(token, QByteArray(16, 'b'), ssid, "profile", wlan_notification_acm_connection_complete, 0) == EventResult::Ignore);
    CHECK(state.event(token, adapter, "other", "profile", wlan_notification_acm_connection_complete, 0) == EventResult::Ignore);
    CHECK(state.event(token, adapter, ssid, "other", wlan_notification_acm_connection_complete, 0) == EventResult::Ignore);
    CHECK(state.event(token, adapter, ssid, "profile", wlan_notification_acm_scan_complete, 0) == EventResult::Ignore);
    state.finish(); CHECK(state.event(token, adapter, ssid, "profile", wlan_notification_acm_connection_complete, 0) == EventResult::Ignore);
    CHECK(!state.expire(token));
    state.begin(adapter, ssid, "profile");
    CHECK(!state.expire(token) && state.active);
    CHECK(state.event(token, adapter, ssid, "profile", wlan_notification_acm_connection_complete, 0) == EventResult::Ignore);
    CHECK(state.expire(state.generation) && !state.active);
    std::cout << "PASS Wi-Fi state: attempt vs final failure, completion, adapter/SSID/profile correlation, stale generation, timeout/finished result guards; no WLAN APIs invoked\n";
    return 0;
}
