#include "include/systemtraymanager.h"

#include <Windows.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <propsys.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <wininet.h>
#include <QFileInfo>
#include <QVariantMap>
#include <iterator>

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
    resetAudioEndpoint();
}

int SystemTrayManager::volume() const { return m_volume; }
bool SystemTrayManager::muted() const { return m_muted; }
QString SystemTrayManager::audioDeviceName() const { return m_audioDeviceName; }
QVariantList SystemTrayManager::audioSessions() const { return m_audioSessions; }
bool SystemTrayManager::online() const { return m_online; }
bool SystemTrayManager::batteryPresent() const { return m_batteryPresent; }
int SystemTrayManager::batteryPercent() const { return m_batteryPercent; }
bool SystemTrayManager::charging() const { return m_charging; }

void SystemTrayManager::setVolume(int volume)
{
    volume = qBound(0, volume, 100);
    if (!ensureAudioEndpoint())
        return;
    if (FAILED(m_audioEndpoint->SetMasterVolumeLevelScalar(volume / 100.0f, nullptr))) {
        resetAudioEndpoint();
        return;
    }
    refreshAudio();
}

void SystemTrayManager::toggleMuted()
{
    if (!ensureAudioEndpoint())
        return;
    if (FAILED(m_audioEndpoint->SetMute(!m_muted, nullptr))) {
        resetAudioEndpoint();
        return;
    }
    refreshAudio();
}

void SystemTrayManager::setSessionVolume(const QString &sessionId, int volume)
{
    updateSession(sessionId, qBound(0, volume, 100), false);
}

void SystemTrayManager::toggleSessionMuted(const QString &sessionId)
{
    updateSession(sessionId, 0, true);
}

void SystemTrayManager::refresh()
{
    refreshAudio();
    refreshPower();
    refreshNetwork();
}

bool SystemTrayManager::ensureAudioEndpoint()
{
    if (m_audioEndpoint)
        return true;

    IMMDeviceEnumerator *enumerator = nullptr;
    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator));
    if (SUCCEEDED(result))
        result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_audioDevice);
    if (SUCCEEDED(result))
        result = m_audioDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                         reinterpret_cast<void **>(&m_audioEndpoint));
    if (SUCCEEDED(result)) {
        IPropertyStore *properties = nullptr;
        if (SUCCEEDED(m_audioDevice->OpenPropertyStore(STGM_READ, &properties))) {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))
                && value.vt == VT_LPWSTR) {
                m_audioDeviceName = QString::fromWCharArray(value.pwszVal);
            }
            PropVariantClear(&value);
            properties->Release();
        }
    }
    if (enumerator)
        enumerator->Release();
    if (FAILED(result))
        resetAudioEndpoint();
    return SUCCEEDED(result) && m_audioEndpoint;
}

void SystemTrayManager::refreshNetwork()
{
    DWORD flags = 0;
    const bool online = InternetGetConnectedState(&flags, 0) != FALSE;
    if (online == m_online)
        return;
    m_online = online;
    emit networkChanged();
}

void SystemTrayManager::refreshAudio()
{
    if (!ensureAudioEndpoint())
        return;
    float scalar = 0.0f;
    BOOL muted = FALSE;
    if (FAILED(m_audioEndpoint->GetMasterVolumeLevelScalar(&scalar))
        || FAILED(m_audioEndpoint->GetMute(&muted))) {
        resetAudioEndpoint();
        return;
    }
    const int volume = qRound(scalar * 100.0f);
    const bool isMuted = muted != FALSE;
    if (volume != m_volume || isMuted != m_muted) {
        m_volume = volume;
        m_muted = isMuted;
        emit audioChanged();
    }
    refreshAudioSessions();
}

void SystemTrayManager::refreshAudioSessions()
{
    if (!m_audioDevice)
        return;
    IAudioSessionManager2 *manager = nullptr;
    IAudioSessionEnumerator *enumerator = nullptr;
    QVariantList sessions;
    HRESULT result = m_audioDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                              reinterpret_cast<void **>(&manager));
    if (SUCCEEDED(result))
        result = manager->GetSessionEnumerator(&enumerator);
    int count = 0;
    if (SUCCEEDED(result))
        enumerator->GetCount(&count);

    for (int index = 0; index < count; ++index) {
        IAudioSessionControl *control = nullptr;
        IAudioSessionControl2 *control2 = nullptr;
        ISimpleAudioVolume *sessionVolume = nullptr;
        if (FAILED(enumerator->GetSession(index, &control))
            || FAILED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void **>(&control2)))
            || FAILED(control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void **>(&sessionVolume)))) {
            if (sessionVolume) sessionVolume->Release();
            if (control2) control2->Release();
            if (control) control->Release();
            continue;
        }

        AudioSessionState state = AudioSessionStateExpired;
        control->GetState(&state);
        if (state != AudioSessionStateExpired) {
            LPWSTR identifier = nullptr;
            LPWSTR displayName = nullptr;
            DWORD processId = 0;
            float scalar = 0.0f;
            BOOL muted = FALSE;
            control2->GetSessionInstanceIdentifier(&identifier);
            control->GetDisplayName(&displayName);
            control2->GetProcessId(&processId);
            sessionVolume->GetMasterVolume(&scalar);
            sessionVolume->GetMute(&muted);

            QString name = displayName ? QString::fromWCharArray(displayName) : QString();
            if (name.isEmpty() && control2->IsSystemSoundsSession() == S_OK) {
                name = QStringLiteral("系统声音");
            } else if (name.isEmpty() && processId != 0) {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
                if (process) {
                    wchar_t path[32768]{};
                    DWORD pathLength = static_cast<DWORD>(std::size(path));
                    if (QueryFullProcessImageNameW(process, 0, path, &pathLength))
                        name = QFileInfo(QString::fromWCharArray(path, pathLength)).completeBaseName();
                    CloseHandle(process);
                }
            }
            if (name.isEmpty())
                name = QStringLiteral("音频会话");

            QVariantMap item;
            item.insert(QStringLiteral("id"), identifier ? QString::fromWCharArray(identifier) : QString::number(index));
            item.insert(QStringLiteral("name"), name);
            item.insert(QStringLiteral("volume"), qRound(scalar * 100.0f));
            item.insert(QStringLiteral("muted"), muted != FALSE);
            item.insert(QStringLiteral("active"), state == AudioSessionStateActive);
            sessions.append(item);
            if (identifier) CoTaskMemFree(identifier);
            if (displayName) CoTaskMemFree(displayName);
        }
        sessionVolume->Release();
        control2->Release();
        control->Release();
    }
    if (enumerator) enumerator->Release();
    if (manager) manager->Release();
    if (sessions != m_audioSessions) {
        m_audioSessions = sessions;
        emit audioSessionsChanged();
    }
}

bool SystemTrayManager::updateSession(const QString &sessionId, int volume, bool toggleMute)
{
    if (!ensureAudioEndpoint() || !m_audioDevice)
        return false;
    IAudioSessionManager2 *manager = nullptr;
    IAudioSessionEnumerator *enumerator = nullptr;
    HRESULT result = m_audioDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                              reinterpret_cast<void **>(&manager));
    if (SUCCEEDED(result)) result = manager->GetSessionEnumerator(&enumerator);
    int count = 0;
    if (SUCCEEDED(result)) enumerator->GetCount(&count);
    bool updated = false;
    for (int index = 0; index < count && !updated; ++index) {
        IAudioSessionControl *control = nullptr;
        IAudioSessionControl2 *control2 = nullptr;
        ISimpleAudioVolume *sessionVolume = nullptr;
        if (SUCCEEDED(enumerator->GetSession(index, &control))
            && SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void **>(&control2)))
            && SUCCEEDED(control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void **>(&sessionVolume)))) {
            LPWSTR identifier = nullptr;
            if (SUCCEEDED(control2->GetSessionInstanceIdentifier(&identifier))) {
                const bool matches = QString::fromWCharArray(identifier) == sessionId;
                CoTaskMemFree(identifier);
                if (matches) {
                    if (toggleMute) {
                        BOOL muted = FALSE;
                        sessionVolume->GetMute(&muted);
                        updated = SUCCEEDED(sessionVolume->SetMute(!muted, nullptr));
                    } else {
                        updated = SUCCEEDED(sessionVolume->SetMasterVolume(volume / 100.0f, nullptr));
                    }
                }
            }
        }
        if (sessionVolume) sessionVolume->Release();
        if (control2) control2->Release();
        if (control) control->Release();
    }
    if (enumerator) enumerator->Release();
    if (manager) manager->Release();
    if (updated) refreshAudioSessions();
    return updated;
}

void SystemTrayManager::resetAudioEndpoint()
{
    if (m_audioEndpoint) {
        m_audioEndpoint->Release();
        m_audioEndpoint = nullptr;
    }
    if (m_audioDevice) {
        m_audioDevice->Release();
        m_audioDevice = nullptr;
    }
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
