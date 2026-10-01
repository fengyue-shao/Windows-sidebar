#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <Windows.h>
#include <shobjidl.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Foundation.h>

// Silent, opt-in desktop SMTC source. No audio stream or persistent registration.
int main(int argc, char **argv) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    QCoreApplication app(argc,argv);
    const QString tag = app.arguments().value(1,"A");
    const auto appId = (QStringLiteral("WindowsSidebar.MediaFixture")+tag).toStdWString();
    SetCurrentProcessExplicitAppUserModelID(appId.c_str());
    HWND window = CreateWindowExW(0,L"STATIC",L"Sidebar silent media fixture",WS_OVERLAPPED,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!window) return 1;
    using namespace winrt::Windows::Media;
    try {
        auto interop = winrt::get_activation_factory<SystemMediaTransportControls,ISystemMediaTransportControlsInterop>();
        SystemMediaTransportControls controls{nullptr};
        winrt::check_hresult(interop->GetForWindow(window,winrt::guid_of<SystemMediaTransportControls>(),winrt::put_abi(controls)));
        SystemMediaTransportControlsTimelineProperties timeline;
        timeline.StartTime(std::chrono::seconds(0)); timeline.EndTime(std::chrono::seconds(120));
        timeline.MinSeekTime(std::chrono::seconds(0)); timeline.MaxSeekTime(std::chrono::seconds(120)); timeline.Position(std::chrono::seconds(5));
        controls.IsPlayEnabled(true); controls.IsPauseEnabled(true); controls.IsNextEnabled(true); controls.IsPreviousEnabled(true);
        const auto positionToken = controls.PlaybackPositionChangeRequested([&](auto&&, const PlaybackPositionChangeRequestedEventArgs &args) {
            const auto position = args.RequestedPlaybackPosition();
            QMetaObject::invokeMethod(&app,[&,position] { timeline.Position(position); controls.UpdateTimelineProperties(timeline); qInfo() << "FIXTURE_SEEK" << position.count()/10000000.0; },Qt::QueuedConnection);
        });
        const auto buttonToken = controls.ButtonPressed([&](auto&&, const SystemMediaTransportControlsButtonPressedEventArgs &args) {
            const auto button = args.Button();
            QMetaObject::invokeMethod(&app,[&,button] { controls.PlaybackStatus(button == SystemMediaTransportControlsButton::Play ? MediaPlaybackStatus::Playing : MediaPlaybackStatus::Paused); },Qt::QueuedConnection);
        });
        auto display = controls.DisplayUpdater(); display.Type(MediaPlaybackType::Music);
        display.MusicProperties().Title(winrt::hstring((QStringLiteral("Silent fixture ")+tag).toStdWString()));
        display.MusicProperties().Artist(L"Windows Sidebar integration test"); display.Update();
        controls.PlaybackStatus(MediaPlaybackStatus::Paused); controls.UpdateTimelineProperties(timeline); controls.IsEnabled(true);
        QTimer::singleShot(90000,&app,&QCoreApplication::quit);
        qInfo() << "FIXTURE_READY" << tag;
        const int result = app.exec();
        controls.PlaybackPositionChangeRequested(positionToken); controls.ButtonPressed(buttonToken); controls.IsEnabled(false);
        DestroyWindow(window); return result;
    } catch (const winrt::hresult_error &error) {
        qCritical() << "FIXTURE_ERROR" << QString::fromWCharArray(error.message().c_str());
        DestroyWindow(window); return 2;
    }
}
