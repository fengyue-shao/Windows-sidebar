#include "include/radiomanager.h"
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
using namespace winrt;
using namespace Windows::Devices::Radios;
namespace {
fire_and_forget updateRadios(QPointer<RadioManager> manager, int targetKind, bool targetState)
{
    bool wifi = false, bluetooth = false;
    try {
        if (co_await Radio::RequestAccessAsync() == RadioAccessStatus::Allowed) {
            const auto radios = co_await Radio::GetRadiosAsync();
            for (const auto &radio : radios) {
                if (targetKind >= 0 && static_cast<int>(radio.Kind()) == targetKind)
                    co_await radio.SetStateAsync(targetState ? RadioState::On : RadioState::Off);
                if (radio.Kind() == RadioKind::WiFi) wifi = radio.State() == RadioState::On;
                if (radio.Kind() == RadioKind::Bluetooth) bluetooth = radio.State() == RadioState::On;
            }
        }
    } catch (...) {}
    QMetaObject::invokeMethod(qGuiApp, [manager, wifi, bluetooth] { if (manager) manager->applyState(wifi, bluetooth); }, Qt::QueuedConnection);
}
}
RadioManager::RadioManager(QObject *parent) : QObject(parent) { refresh(); }
void RadioManager::refresh() { updateRadios(this, -1, false); }
void RadioManager::setWifiEnabled(bool enabled) { updateRadios(this, static_cast<int>(RadioKind::WiFi), enabled); }
void RadioManager::setBluetoothEnabled(bool enabled) { updateRadios(this, static_cast<int>(RadioKind::Bluetooth), enabled); }
void RadioManager::applyState(bool wifi, bool bluetooth)
{
    if (wifi == m_wifiEnabled && bluetooth == m_bluetoothEnabled) return;
    m_wifiEnabled = wifi; m_bluetoothEnabled = bluetooth; emit stateChanged();
}
