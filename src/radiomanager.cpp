#include "include/radiomanager.h"
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <chrono>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
using namespace winrt;
using namespace Windows::Devices::Radios;
namespace {
fire_and_forget updateRadios(QPointer<RadioManager> manager, int targetKind, bool targetState)
{
    bool wifi = false, bluetooth = false, valid = false, targetFound = false;
    QString status;
    try {
        if (co_await Radio::RequestAccessAsync() == RadioAccessStatus::Allowed) {
            const auto radios = co_await Radio::GetRadiosAsync();
            for (const auto &radio : radios) {
                if (targetKind >= 0 && static_cast<int>(radio.Kind()) == targetKind) {
                    targetFound = true;
                    const auto access = co_await radio.SetStateAsync(targetState ? RadioState::On : RadioState::Off);
                    if (access != RadioAccessStatus::Allowed) status = QStringLiteral("Windows 拒绝无线开关请求（%1）").arg(static_cast<int>(access));
                    else {
                        // Accepted does not mean the physical transition is finished.
                        for (int attempt = 0; attempt < 8 && radio.State() != (targetState ? RadioState::On : RadioState::Off); ++attempt)
                            co_await winrt::resume_after(std::chrono::milliseconds(250));
                        if (radio.State() != (targetState ? RadioState::On : RadioState::Off))
                            status = QStringLiteral("无线设备尚未到达请求状态，请检查硬件开关或系统策略");
                    }
                }
                if (radio.Kind() == RadioKind::WiFi) wifi |= radio.State() == RadioState::On;
                if (radio.Kind() == RadioKind::Bluetooth) bluetooth |= radio.State() == RadioState::On;
            }
            valid = true;
            if (targetKind >= 0 && !targetFound) status = QStringLiteral("未找到可控制的无线设备");
        } else status = QStringLiteral("没有无线设备控制权限");
    } catch (...) { status = QStringLiteral("无线设备访问失败，已保留上次状态"); }
    QMetaObject::invokeMethod(qGuiApp, [manager, valid, wifi, bluetooth, status] { if (manager) manager->complete(valid, wifi, bluetooth, status); }, Qt::QueuedConnection);
}
}
RadioManager::RadioManager(QObject *parent) : QObject(parent) { refresh(); }
void RadioManager::refresh() { if (!m_busy) dispatch(-1, false); }
void RadioManager::dispatch(int kind, bool enabled) {
    m_busy = true; m_status.clear(); emit stateChanged(); updateRadios(this, kind, enabled);
}
void RadioManager::setWifiEnabled(bool enabled) { m_pendingWifi = enabled; drain(); }
void RadioManager::setBluetoothEnabled(bool enabled) { m_pendingBluetooth = enabled; drain(); }
void RadioManager::drain() {
    if (m_busy) return;
    if (m_pendingWifi >= 0) { const bool state = m_pendingWifi; m_pendingWifi = -1; dispatch(static_cast<int>(RadioKind::WiFi), state); }
    else if (m_pendingBluetooth >= 0) { const bool state = m_pendingBluetooth; m_pendingBluetooth = -1; dispatch(static_cast<int>(RadioKind::Bluetooth), state); }
}
void RadioManager::complete(bool valid, bool wifi, bool bluetooth, const QString &status) {
    if (valid) { m_wifiEnabled = wifi; m_bluetoothEnabled = bluetooth; }
    m_busy = false; m_status = status; emit stateChanged(); drain();
}
void RadioManager::applyState(bool wifi, bool bluetooth)
{
    if (wifi == m_wifiEnabled && bluetooth == m_bluetoothEnabled) return;
    m_wifiEnabled = wifi; m_bluetoothEnabled = bluetooth; emit stateChanged();
}
