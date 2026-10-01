#include "include/audioinputmanager.h"
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <QDesktopServices>
#include <QUrl>
#include <QVariantMap>
#include <cmath>

using Microsoft::WRL::ComPtr;
namespace {
QString deviceId(IMMDevice *device) {
    LPWSTR id = nullptr;
    if (!device || FAILED(device->GetId(&id))) return {};
    const auto result = QString::fromWCharArray(id); CoTaskMemFree(id); return result;
}
QString deviceName(IMMDevice *device) {
    ComPtr<IPropertyStore> store;
    if (!device || FAILED(device->OpenPropertyStore(STGM_READ, &store))) return {};
    PROPVARIANT v; PropVariantInit(&v);
    QString name;
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR) name = QString::fromWCharArray(v.pwszVal);
    PropVariantClear(&v); return name;
}
}
struct AudioInputManager::State {
    ComPtr<IMMDevice> input, output;
    ComPtr<IAudioEndpointVolume> control;
    ComPtr<IAudioMeterInformation> inputMeter, outputMeter;
    QString inputId, outputId, name, status;
    QVariantList devices;
    int volume = 0;
    bool muted = false, enabled = false;
    double inputPeak = 0, outputPeak = 0;
};
AudioInputManager::AudioInputManager(QObject *parent) : QObject(parent), d(std::make_unique<State>()) {
    m_refreshTimer.setInterval(1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AudioInputManager::refresh);
    m_meterTimer.setInterval(100);
    connect(&m_meterTimer, &QTimer::timeout, this, &AudioInputManager::pollLevels);
    refresh(); m_refreshTimer.start();
}
AudioInputManager::~AudioInputManager() = default;
bool AudioInputManager::available() const { return d->control != nullptr; }
QString AudioInputManager::deviceName() const { return d->name; }
int AudioInputManager::volume() const { return d->volume; }
bool AudioInputManager::muted() const { return d->muted; }
QString AudioInputManager::statusText() const { return d->status; }
QVariantList AudioInputManager::devices() const { return d->devices; }
double AudioInputManager::inputPeak() const { return d->inputPeak; }
double AudioInputManager::outputPeak() const { return d->outputPeak; }
bool AudioInputManager::inputMeterAvailable() const { return d->inputMeter != nullptr; }
bool AudioInputManager::outputMeterAvailable() const { return d->outputMeter != nullptr; }
bool AudioInputManager::meteringEnabled() const { return d->enabled; }
void AudioInputManager::setMeteringEnabled(bool enabled) {
    if (enabled == d->enabled) return;
    d->enabled = enabled;
    if (enabled) { refresh(); m_meterTimer.start(); pollLevels(); }
    else { m_meterTimer.stop(); d->inputPeak = d->outputPeak = 0; emit levelsChanged(); }
    emit meteringEnabledChanged();
}
void AudioInputManager::refresh() {
    // Detect default device changes by endpoint ID, including unplug/replug.
    ComPtr<IMMDeviceEnumerator> enumerator;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    ComPtr<IMMDevice> input, output;
    if (enumerator) {
        enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &input);
        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &output);
    }
    const auto inputId = deviceId(input.Get()), outputId = deviceId(output.Get());
    if (inputId != d->inputId || (input && (!d->control || !d->inputMeter))) {
        d->inputId = inputId; d->input = input; d->control.Reset(); d->inputMeter.Reset();
        d->name = ::deviceName(input.Get()); d->volume = 0; d->muted = false;
        if (input) {
            input->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(d->control.GetAddressOf()));
            input->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(d->inputMeter.GetAddressOf()));
        }
    }
    if (outputId != d->outputId || (output && !d->outputMeter)) {
        d->outputId = outputId; d->output = output; d->outputMeter.Reset();
        if (output) output->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(d->outputMeter.GetAddressOf()));
    }
    float scalar = 0; BOOL mute = FALSE;
    if (d->control && SUCCEEDED(d->control->GetMasterVolumeLevelScalar(&scalar)) && SUCCEEDED(d->control->GetMute(&mute))) {
        d->volume = qBound(0, qRound(scalar*100), 100); d->muted = mute != FALSE;
        d->status = QStringLiteral("控制默认输入设备；设备切换请使用 Windows 设置");
    } else {
        d->control.Reset(); d->volume = 0; d->muted = false;
        d->status = input ? QStringLiteral("无法访问输入控制，请检查设备和系统权限") : QStringLiteral("没有可用的默认输入设备");
    }
    d->devices.clear();
    ComPtr<IMMDeviceCollection> collection;
    if (enumerator && SUCCEEDED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection))) {
        UINT count = 0; collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (SUCCEEDED(collection->Item(i, &device)))
                d->devices.append(QVariantMap{{"id", deviceId(device.Get())}, {"name", ::deviceName(device.Get())}, {"selected", deviceId(device.Get()) == inputId}});
        }
    }
    emit inputChanged();
    if (d->enabled) pollLevels();
}
void AudioInputManager::pollLevels() {
    // Reads endpoint meters only: no capture client, buffers, recording or upload.
    float input = 0, output = 0;
    if (d->inputMeter && FAILED(d->inputMeter->GetPeakValue(&input))) d->inputMeter.Reset();
    if (d->outputMeter && FAILED(d->outputMeter->GetPeakValue(&output))) d->outputMeter.Reset();
    d->inputPeak = d->muted || !std::isfinite(input) ? 0 : qBound(0.0, double(input), 1.0);
    d->outputPeak = !std::isfinite(output) ? 0 : qBound(0.0, double(output), 1.0);
    emit levelsChanged();
}
void AudioInputManager::setVolume(int value) {
    refresh();
    if (d->control && FAILED(d->control->SetMasterVolumeLevelScalar(qBound(0,value,100)/100.0f, nullptr))) {
        d->status = QStringLiteral("此设备未接受输入音量调整"); emit inputChanged(); return;
    }
    refresh();
}
void AudioInputManager::toggleMuted() {
    refresh();
    if (d->control && FAILED(d->control->SetMute(!d->muted, nullptr))) {
        d->status = QStringLiteral("此设备未接受静音操作"); emit inputChanged(); return;
    }
    refresh();
}
void AudioInputManager::openInputSettings() {
    if (!QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:sound")))) {
        d->status = QStringLiteral("无法打开 Windows 声音设置"); emit inputChanged();
    }
}
