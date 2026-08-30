#include "include/systemtraymanager.h"

#include <Windows.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &SystemTrayManager::refresh);
    refresh();
    m_timer.start();
}

SystemTrayManager::~SystemTrayManager()
{
    if (m_audioEndpoint)
        m_audioEndpoint->Release();
}

int SystemTrayManager::volume() const { return m_volume; }
bool SystemTrayManager::muted() const { return m_muted; }
bool SystemTrayManager::batteryPresent() const { return m_batteryPresent; }
int SystemTrayManager::batteryPercent() const { return m_batteryPercent; }
bool SystemTrayManager::charging() const { return m_charging; }

void SystemTrayManager::setVolume(int volume)
{
    volume = qBound(0, volume, 100);
    if (!ensureAudioEndpoint())
        return;
    if (FAILED(m_audioEndpoint->SetMasterVolumeLevelScalar(volume / 100.0f, nullptr))) {
        m_audioEndpoint->Release();
        m_audioEndpoint = nullptr;
        return;
    }
    refreshAudio();
}

void SystemTrayManager::toggleMuted()
{
    if (!ensureAudioEndpoint())
        return;
    if (FAILED(m_audioEndpoint->SetMute(!m_muted, nullptr))) {
        m_audioEndpoint->Release();
        m_audioEndpoint = nullptr;
        return;
    }
    refreshAudio();
}

void SystemTrayManager::refresh()
{
    refreshAudio();
    refreshPower();
}

bool SystemTrayManager::ensureAudioEndpoint()
{
    if (m_audioEndpoint)
        return true;

    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator));
    if (SUCCEEDED(result))
        result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (SUCCEEDED(result))
        result = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void **>(&m_audioEndpoint));
    if (device)
        device->Release();
    if (enumerator)
        enumerator->Release();
    return SUCCEEDED(result) && m_audioEndpoint;
}

void SystemTrayManager::refreshAudio()
{
    if (!ensureAudioEndpoint())
        return;
    float scalar = 0.0f;
    BOOL muted = FALSE;
    if (FAILED(m_audioEndpoint->GetMasterVolumeLevelScalar(&scalar))
        || FAILED(m_audioEndpoint->GetMute(&muted))) {
        m_audioEndpoint->Release();
        m_audioEndpoint = nullptr;
        return;
    }
    const int volume = qRound(scalar * 100.0f);
    const bool isMuted = muted != FALSE;
    if (volume == m_volume && isMuted == m_muted)
        return;
    m_volume = volume;
    m_muted = isMuted;
    emit audioChanged();
}

void SystemTrayManager::refreshPower()
{
    SYSTEM_POWER_STATUS status{};
    if (!GetSystemPowerStatus(&status))
        return;
    const bool present = status.BatteryFlag != 128 && status.BatteryLifePercent != 255;
    const int percent = present ? status.BatteryLifePercent : 0;
    const bool charging = present && (status.BatteryFlag & 8) != 0;
    if (present == m_batteryPresent && percent == m_batteryPercent && charging == m_charging)
        return;
    m_batteryPresent = present;
    m_batteryPercent = percent;
    m_charging = charging;
    emit powerChanged();
}
