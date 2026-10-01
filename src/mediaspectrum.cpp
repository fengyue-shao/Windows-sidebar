#include "include/mediaspectrum.h"
#include "include/spectrumanalyzer.h"
#include <QThread>
#include <QElapsedTimer>
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <atomic>
#include <cstring>

using Microsoft::WRL::ComPtr;
class MediaSpectrum::CaptureThread : public QThread {
public:
    std::atomic_bool enabled{false};
    MediaSpectrum *owner;
    explicit CaptureThread(MediaSpectrum *receiver) : owner(receiver) {}
    void publish(const QVariantList &bands, bool available, const QString &status) {
        QMetaObject::invokeMethod(owner,[receiver=owner,bands,available,status] {
            if (!receiver->m_enabled) return;
            receiver->m_bands=bands; receiver->m_available=available; receiver->m_status=status;
            emit receiver->spectrumChanged();
        },Qt::QueuedConnection);
    }
    void run() override {
        const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        if (FAILED(com)) { publish({},false,QStringLiteral("无法初始化音频分析线程")); return; }
        {
            ComPtr<IMMDeviceEnumerator> enumerator;
            ComPtr<IAudioClient> client;
            ComPtr<IAudioCaptureClient> capture;
            QString currentId;
            WAVEFORMATEX *format=nullptr;
            bool floating=false;
            int sampleBytes=0;
            SpectrumAnalyzer::Samples samples{};
            int cursor=0, collected=0;
            std::array<double,SpectrumAnalyzer::Bands> smoothed{};
            QElapsedTimer deviceCheck, publishClock, lastPacket;
            deviceCheck.start(); publishClock.start(); lastPacket.start();
            auto close=[&] {
                if (client) client->Stop();
                capture.Reset(); client.Reset();
                if (format) { CoTaskMemFree(format); format=nullptr; }
                currentId.clear(); samples.fill(0); smoothed.fill(0); cursor=collected=0;
            };
            while (!isInterruptionRequested()) {
                if (!enabled.load()) { if (client) close(); msleep(20); continue; }
                if (!client || deviceCheck.elapsed()>=1000) {
                    deviceCheck.restart();
                    if (!enumerator) CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
                    ComPtr<IMMDevice> device;
                    HRESULT hr=enumerator ? enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device) : E_FAIL;
                    LPWSTR id=nullptr;
                    if (SUCCEEDED(hr)) hr=device->GetId(&id);
                    const QString nextId=id ? QString::fromWCharArray(id) : QString();
                    CoTaskMemFree(id);
                    if (FAILED(hr) || nextId!=currentId || !client) {
                        close();
                        if (SUCCEEDED(hr)) hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void **>(client.GetAddressOf()));
                        if (SUCCEEDED(hr)) hr=client->GetMixFormat(&format);
                        if (SUCCEEDED(hr)) {
                            WORD tag=format->wFormatTag;
                            if (tag==WAVE_FORMAT_EXTENSIBLE && format->cbSize>=22) {
                                const auto *extended=reinterpret_cast<WAVEFORMATEXTENSIBLE *>(format);
                                tag=extended->SubFormat==KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ? WAVE_FORMAT_IEEE_FLOAT : extended->SubFormat==KSDATAFORMAT_SUBTYPE_PCM ? WAVE_FORMAT_PCM : 0;
                            }
                            floating=tag==WAVE_FORMAT_IEEE_FLOAT;
                            sampleBytes=format->wBitsPerSample/8;
                            if (!format->nChannels || format->nChannels>32 || format->nSamplesPerSec<8000 || format->nSamplesPerSec>384000
                                || format->nBlockAlign!=format->nChannels*sampleBytes
                                || !((floating && sampleBytes==4) || (tag==WAVE_FORMAT_PCM && (sampleBytes==2 || sampleBytes==3 || sampleBytes==4)))) hr=AUDCLNT_E_UNSUPPORTED_FORMAT;
                        }
                        if (SUCCEEDED(hr)) hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,format,nullptr);
                        if (SUCCEEDED(hr)) hr=client->GetService(IID_PPV_ARGS(&capture));
                        if (SUCCEEDED(hr)) hr=client->Start();
                        if (FAILED(hr)) {
                            close(); publish({},false,QStringLiteral("输出频谱暂不可用 (0x%1)").arg(quint32(hr),8,16,QChar('0')));
                            // Retry with a bounded wait, still responsive to shutdown/disable.
                            for (int i=0;i<50 && enabled.load() && !isInterruptionRequested();++i) msleep(20);
                            continue;
                        }
                        currentId=nextId; lastPacket.restart();
                        publish({},true,QStringLiteral("系统输出频谱 · 包含其他应用声音"));
                    }
                }
                UINT32 packet=0;
                HRESULT hr=capture->GetNextPacketSize(&packet);
                int drained=0;
                while (SUCCEEDED(hr) && packet && drained++<64 && enabled.load() && !isInterruptionRequested()) {
                    BYTE *data=nullptr; UINT32 frames=0; DWORD flags=0;
                    hr=capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr);
                    if (FAILED(hr)) break;
                    if (flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) { samples.fill(0); cursor=collected=0; }
                    for (UINT32 frame=0;frame<frames;++frame) {
                        double value=0;
                        if (!(flags&AUDCLNT_BUFFERFLAGS_SILENT) && data) {
                            for (int channel=0;channel<format->nChannels;++channel) {
                                const BYTE *sample=data+frame*format->nBlockAlign+channel*sampleBytes;
                                double decoded=0;
                                if (floating) { float f; std::memcpy(&f,sample,4); decoded=std::isfinite(f) ? f : 0; }
                                else if (sampleBytes==2) { int16_t n; std::memcpy(&n,sample,2); decoded=n/32768.0; }
                                else if (sampleBytes==3) { int32_t n=sample[0]|(sample[1]<<8)|(sample[2]<<16); if (n&0x800000) n|=~0xffffff; decoded=n/8388608.0; }
                                else { int32_t n; std::memcpy(&n,sample,4); decoded=n/2147483648.0; }
                                value+=decoded;
                            }
                            value/=format->nChannels;
                        }
                        samples[cursor]=float(std::clamp(value,-1.0,1.0)); cursor=(cursor+1)%SpectrumAnalyzer::Size; collected=(std::min)(collected+1,SpectrumAnalyzer::Size);
                    }
                    hr=capture->ReleaseBuffer(frames); lastPacket.restart();
                    if (SUCCEEDED(hr)) hr=capture->GetNextPacketSize(&packet);
                }
                if (FAILED(hr)) { close(); publish({},false,QStringLiteral("输出设备已改变，正在重试")); continue; }
                if (publishClock.elapsed()>=40) {
                    publishClock.restart();
                    SpectrumAnalyzer::Samples ordered{};
                    if (collected==SpectrumAnalyzer::Size && lastPacket.elapsed()<160)
                        for (int i=0;i<SpectrumAnalyzer::Size;++i) ordered[i]=samples[(cursor+i)%SpectrumAnalyzer::Size];
                    const auto values=SpectrumAnalyzer::analyze(ordered,int(format->nSamplesPerSec));
                    QVariantList bands; bands.reserve(SpectrumAnalyzer::Bands);
                    for (int i=0;i<SpectrumAnalyzer::Bands;++i) { smoothed[i]+=(values[i]-smoothed[i])*(values[i]>smoothed[i] ? 0.75 : 0.22); bands.append(smoothed[i]); }
                    publish(bands,true,QStringLiteral("系统输出频谱 · 包含其他应用声音"));
                }
                msleep(10);
            }
            close();
        }
        CoUninitialize();
    }
};
MediaSpectrum::MediaSpectrum(QObject *parent) : QObject(parent), m_thread(std::make_unique<CaptureThread>(this)) { m_thread->start(); }
MediaSpectrum::~MediaSpectrum() { m_thread->requestInterruption(); m_thread->wait(); }
void MediaSpectrum::setRequested(bool value) { if (m_requested==value) return; m_requested=value; emit requestedChanged(); }
void MediaSpectrum::setEnabled(bool value) {
    if (m_enabled==value) return;
    m_enabled=value; m_thread->enabled.store(value);
    if (!value) { m_bands.clear(); m_available=false; m_status.clear(); emit spectrumChanged(); }
    emit enabledChanged();
}
