#include "include/captureusagemonitor.h"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QVariantMap>
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <wrl.h>
#include <array>
#include <vector>
#include <map>
#include <algorithm>
using Microsoft::WRL::ComPtr;
namespace {
class SessionSink : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,IAudioSessionNotification> {
public:
    QMutex mutex;
    std::vector<ComPtr<IAudioSessionControl>> pending;
    HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl *control) override {
        if (!control) return E_POINTER;
        QMutexLocker lock(&mutex);
        try { if (pending.size()<256) pending.emplace_back(control); }
        catch (...) { return E_OUTOFMEMORY; }
        return S_OK;
    }
};
QString processName(DWORD pid) {
    if (!pid) return QStringLiteral("未知应用");
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    std::array<wchar_t,32768> path{}; DWORD length=DWORD(path.size());
    const bool ok=process && QueryFullProcessImageNameW(process,0,path.data(),&length);
    if (process) CloseHandle(process);
    return ok ? QFileInfo(QString::fromWCharArray(path.data(),int(length))).completeBaseName().left(80) : QStringLiteral("未知应用 (PID %1)").arg(pid);
}
struct Endpoint {
    ComPtr<IAudioSessionManager2> manager;
    ComPtr<SessionSink> sink;
    QHash<QString,ComPtr<IAudioSessionControl>> sessions;
    ~Endpoint() { if (manager && sink) manager->UnregisterSessionNotification(sink.Get()); }
    void add(IAudioSessionControl *control) {
        ComPtr<IAudioSessionControl2> extended;
        if (FAILED(control->QueryInterface(IID_PPV_ARGS(&extended)))) return;
        LPWSTR id=nullptr;
        if (SUCCEEDED(extended->GetSessionInstanceIdentifier(&id))) {
            if (sessions.size()<256 || sessions.contains(QString::fromWCharArray(id))) sessions.insert(QString::fromWCharArray(id),ComPtr<IAudioSessionControl>(control));
        }
        CoTaskMemFree(id);
    }
};
}
class CaptureUsageMonitor::Worker : public QThread {
public:
    CaptureUsageMonitor *owner;
    explicit Worker(CaptureUsageMonitor *value) : owner(value) {}
    void run() override {
        if (FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED))) return;
        {
            ComPtr<IMMDeviceEnumerator> enumerator;
            std::map<QString,std::unique_ptr<Endpoint>> endpoints;
            while (!isInterruptionRequested()) {
                if (!enumerator) CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
                ComPtr<IMMDeviceCollection> collection;
                const HRESULT hr=enumerator ? enumerator->EnumAudioEndpoints(eCapture,DEVICE_STATE_ACTIVE,&collection) : E_FAIL;
                QVariantList apps; QSet<QString> seenDevices, seenApps;
                bool available=SUCCEEDED(hr), partial=false;
                UINT count=0; if (collection) collection->GetCount(&count);
                for (UINT i=0;i<count;++i) {
                    ComPtr<IMMDevice> device; LPWSTR rawId=nullptr;
                    if (FAILED(collection->Item(i,&device)) || FAILED(device->GetId(&rawId))) { partial=true; continue; }
                    const auto id=QString::fromWCharArray(rawId); CoTaskMemFree(rawId); seenDevices.insert(id);
                    if (!endpoints.contains(id)) {
                        auto endpoint=std::make_unique<Endpoint>();
                        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void **>(endpoint->manager.GetAddressOf())))) { partial=true; continue; }
                        endpoint->sink=Microsoft::WRL::Make<SessionSink>();
                        if (FAILED(endpoint->manager->RegisterSessionNotification(endpoint->sink.Get()))) { partial=true; continue; }
                        endpoints.emplace(id,std::move(endpoint));
                    }
                    auto &endpoint=*endpoints.at(id);
                    ComPtr<IAudioSessionEnumerator> sessions;
                    if (SUCCEEDED(endpoint.manager->GetSessionEnumerator(&sessions))) {
                        // GetCount is also required before session notifications arrive.
                        int size=0; sessions->GetCount(&size);
                        for (int n=0;n<size;++n) { ComPtr<IAudioSessionControl> control; if (SUCCEEDED(sessions->GetSession(n,&control))) endpoint.add(control.Get()); }
                    } else partial=true;
                    std::vector<ComPtr<IAudioSessionControl>> pending;
                    {
                        QMutexLocker lock(&endpoint.sink->mutex);
                        pending.swap(endpoint.sink->pending);
                    }
                    for (auto &control:pending) endpoint.add(control.Get());
                    for (auto it=endpoint.sessions.begin();it!=endpoint.sessions.end();) {
                        AudioSessionState state;
                        if (FAILED(it.value()->GetState(&state)) || state==AudioSessionStateExpired) { it=endpoint.sessions.erase(it); continue; }
                        if (state==AudioSessionStateActive) {
                            ComPtr<IAudioSessionControl2> control;
                            it.value()->QueryInterface(IID_PPV_ARGS(&control));
                            DWORD pid=0; const HRESULT source=control ? control->GetProcessId(&pid) : E_FAIL;
                            const bool uncertain=source!=S_OK || pid==0;
                            const QString appId=uncertain ? "unknown."+id+"."+it.key() : QString::number(pid);
                            if (!seenApps.contains(appId)) {
                                seenApps.insert(appId);
                                const QString name=!uncertain ? processName(pid) : QStringLiteral("跨进程或未知来源");
                                apps.append(QVariantMap{{"id",appId},{"pid",quint32(pid)},{"name",name},{"uncertain",uncertain}});
                            }
                        }
                        ++it;
                    }
                }
                for (auto it=endpoints.begin();it!=endpoints.end();) it=!seenDevices.contains(it->first) ? endpoints.erase(it) : ++it;
                std::sort(apps.begin(),apps.end(),[](const QVariant &a,const QVariant &b) { return a.toMap().value("id").toString()<b.toMap().value("id").toString(); });
                const QString status=!available ? QStringLiteral("无法读取输入会话") : partial ? QStringLiteral("部分输入端点无法读取；识别覆盖有限") : QStringLiteral("标准捕获会话检测；网页、代理与独占模式识别有限");
                const int observed=int(endpoints.size());
                QMetaObject::invokeMethod(owner,[receiver=owner,apps,available,status,observed] {
                    if (receiver->m_apps==apps && receiver->m_available==available && receiver->m_status==status && receiver->m_observedEndpoints==observed) return;
                    receiver->m_apps=apps; receiver->m_available=available; receiver->m_status=status; receiver->m_observedEndpoints=observed; emit receiver->usageChanged();
                },Qt::QueuedConnection);
                for (int n=0;n<10 && !isInterruptionRequested();++n) msleep(50);
            }
        }
        CoUninitialize();
    }
};
CaptureUsageMonitor::CaptureUsageMonitor(QObject *parent) : QObject(parent),m_worker(std::make_unique<Worker>(this)) { m_worker->start(); }
CaptureUsageMonitor::~CaptureUsageMonitor() { m_worker->requestInterruption(); m_worker->wait(); }
