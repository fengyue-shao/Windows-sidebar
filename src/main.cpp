#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QMetaType>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <winrt/base.h>

#include "include/coverimageprovider.h"
#include "include/fluenticonprovider.h"
#include "include/audioinputmanager.h"
#include "include/mediaspectrum.h"
#include "include/islandbridge.h"
#include "include/captureusagemonitor.h"
#include "include/bluetoothmanager.h"
#include "include/clipboardmanager.h"
#include "include/clipboardocr.h"
#include "include/cpumonitor.h"
#include "include/diskmonitor.h"
#include "include/displaymanager.h"
#include "include/dropdowncontroller.h"
#include "include/focustimer.h"
#include "include/gpumonitor.h"
#include "include/mediamonitor.h"
#include "include/memorymonitor.h"
#include "include/monitorscheduler.h"
#include "include/networkmonitor.h"
#include "include/networkdetails.h"
#include "include/hotspotmanager.h"
#include "include/awakemanager.h"
#include "include/usbmanager.h"
#include "include/featuretests.h"
#include "include/radiomanager.h"
#include "include/pluginregistry.h"
#include "include/terminalmanager.h"
#include "include/wifimanager.h"
#include "include/notificationlistener.h"
#include "include/notificationmodel.h"
#include "include/systemtraymanager.h"
#include "include/thememanager.h"
#include "include/uisettings.h"

#include "include/terminalcanvas.h"

int main(int argc, char *argv[])
{
    qmlRegisterType<TerminalCanvas>("WindowsSidebar.Terminal", 1, 0, "TerminalCanvas");
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion("0.8.1.3-preview");
    QCoreApplication::setOrganizationName("WindowsSidebar");
    QCoreApplication::setApplicationName("WindowsSidebar");
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<Notification>("Notification");
    for (const auto &argument : app.arguments())
        if (argument.startsWith("--feature-self-test=")) return runFeatureTests(app, argument.mid(20));

    ClipboardManager clipboardManager;
    ClipboardOcr clipboardOcr;
    BluetoothManager bluetoothManager;
    CpuMonitor cpuMonitor;
    GpuMonitor gpuMonitor;
    DiskMonitor diskMonitor;
    DisplayManager displayManager;
    FocusTimer focusTimer;
    MemoryMonitor memoryMonitor;
    NetworkMonitor networkMonitor;
    NetworkDetails networkDetails;
    HotspotManager hotspotManager;
    AwakeManager awakeManager;
    UsbManager usbManager;
    RadioManager radioManager;
    PluginRegistry pluginRegistry;
    TerminalManager terminalManager;
    WifiManager wifiManager;
    MediaMonitor mediaMonitor;
    NotificationModel notificationModel;
    NotificationListener notificationListener;
    SystemTrayManager systemTrayManager;
    AudioInputManager audioInputManager;
    MediaSpectrum mediaSpectrum;
    MonitorScheduler monitorScheduler;
    ThemeManager themeManager;
    UiSettings uiSettings;
    DropdownController dropdownController;
    IslandBridge islandBridge;
    CaptureUsageMonitor captureUsageMonitor;
    // Destroy the QML engine before its context services, avoiding null context
    // bindings and active callbacks during application teardown.
    QQmlApplicationEngine engine;

    monitorScheduler.addModule(&cpuMonitor, 1000);
    monitorScheduler.addModule(&memoryMonitor, 1000);
    monitorScheduler.addModule(&gpuMonitor, 1000);
    monitorScheduler.addModule(&networkMonitor, 1000);
    monitorScheduler.addModule(&diskMonitor, 5000);
    monitorScheduler.start();

    QObject::connect(&notificationListener, &NotificationListener::notificationAdded,
                     &notificationModel, &NotificationModel::addNotification);
    QObject::connect(&notificationListener,&NotificationListener::notificationArrived,&islandBridge,[&](const Notification &notice) {
        if (!uiSettings.notificationsEnabled()) return;
        islandBridge.pushEvent("builtin.notification",notice.id,{{"title",notice.appName.isEmpty() ? QStringLiteral("新通知") : notice.appName+QStringLiteral(" · 新通知")},
            {"subtitle",QStringLiteral("点击打开通知中心")},{"iconName","alert"},{"route","notifications"},{"kind","notification"}},80,4000,15000);
    });
    QObject::connect(&notificationListener, &NotificationListener::notificationUpdated,
                     &notificationModel, &NotificationModel::addNotification);
    QObject::connect(&notificationListener, &NotificationListener::notificationRemoved,
                     &notificationModel, &NotificationModel::removeById);
    QObject::connect(&notificationModel, &NotificationModel::removalRequested,
                     &notificationListener, &NotificationListener::removeNotification);
    QObject::connect(&notificationModel, &NotificationModel::clearRequested,
                     &notificationListener, &NotificationListener::clearNotifications);
    QObject::connect(&notificationListener, &NotificationListener::accessDenied,
                     [] {
                         qWarning() << "Access to Windows notifications was denied";
                     });
    QObject::connect(&notificationListener, &NotificationListener::errorOccurred,
                     [](const QString &message) {
                         qWarning() << "Windows notification listener:" << message;
                     });
    notificationListener.start();

    auto *coverProvider = new CoverImageProvider;
    mediaMonitor.setCoverProvider(coverProvider);
    engine.rootContext()->setContextProperty("cpuMonitor", &cpuMonitor);
    engine.rootContext()->setContextProperty("clipboardManager", &clipboardManager);
    engine.rootContext()->setContextProperty("clipboardOcr", &clipboardOcr);
    engine.rootContext()->setContextProperty("bluetoothManager", &bluetoothManager);
    engine.rootContext()->setContextProperty("gpuMonitor", &gpuMonitor);
    engine.rootContext()->setContextProperty("diskMonitor", &diskMonitor);
    engine.rootContext()->setContextProperty("displayManager", &displayManager);
    engine.rootContext()->setContextProperty("focusTimer", &focusTimer);
    engine.rootContext()->setContextProperty("memoryMonitor", &memoryMonitor);
    engine.rootContext()->setContextProperty("networkMonitor", &networkMonitor);
    engine.rootContext()->setContextProperty("networkDetails", &networkDetails);
    engine.rootContext()->setContextProperty("hotspotManager", &hotspotManager);
    engine.rootContext()->setContextProperty("awakeManager", &awakeManager);
    engine.rootContext()->setContextProperty("usbManager", &usbManager);
    engine.rootContext()->setContextProperty("radioManager", &radioManager);
    engine.rootContext()->setContextProperty("pluginRegistry", &pluginRegistry);
    engine.rootContext()->setContextProperty("terminalManager", &terminalManager);
    engine.rootContext()->setContextProperty("wifiManager", &wifiManager);
    engine.rootContext()->setContextProperty("mediaMonitor", &mediaMonitor);
    engine.rootContext()->setContextProperty("notificationModel", &notificationModel);
    engine.rootContext()->setContextProperty("systemTrayManager", &systemTrayManager);
    engine.rootContext()->setContextProperty("audioInputManager", &audioInputManager);
    engine.rootContext()->setContextProperty("mediaSpectrum", &mediaSpectrum);
    engine.rootContext()->setContextProperty("islandBridge", &islandBridge);
    engine.rootContext()->setContextProperty("captureUsageMonitor", &captureUsageMonitor);
    engine.rootContext()->setContextProperty("themeManager", &themeManager);
    engine.rootContext()->setContextProperty("uiSettings", &uiSettings);
    engine.rootContext()->setContextProperty("dropdownController", &dropdownController);
    engine.rootContext()->setContextProperty("previewCards", app.arguments().contains("--preview-cards"));
    engine.rootContext()->setContextProperty("uiSelfTest", app.arguments().contains("--ui-self-test"));
    QString uiCapturePath;
    for (const auto &arg : app.arguments()) if (arg.startsWith("--ui-capture=")) uiCapturePath = arg.mid(13);
    engine.rootContext()->setContextProperty("uiCapturePath", uiCapturePath);
    engine.addImageProvider("cover", coverProvider);
    auto *fluentProvider = new FluentIconProvider;
    engine.addImageProvider("fluent", fluentProvider);
    if (app.arguments().contains("--ui-self-test")) {
        clipboardOcr.recognize(QString());
        clipboardOcr.finish(99, "stale", "stale");
        if (clipboardOcr.busy() || !clipboardOcr.text().isEmpty() || !clipboardOcr.status().isEmpty()) return 7;
        clipboardOcr.copy(QString()); // Empty results never alter the clipboard.
        quint64 testNow = 1000;
        int testRequests = 0;
        AwakeManager testAwake(nullptr, [&](bool, bool) { ++testRequests; return true; }, [&] { return testNow; });
        testAwake.start(0, false);
        if (testRequests != 0 || testAwake.active()) return 9;
        testAwake.start(1, false);
        if (!testAwake.active() || testAwake.remainingSeconds() != 60 || testRequests != 1) return 9;
        testNow += 60000;
        QMetaObject::invokeMethod(&testAwake, "tick", Qt::DirectConnection);
        if (testAwake.active() || testAwake.remainingSeconds() != 0 || testRequests != 2) return 9;
        if (!HotspotManager::validConfiguration("test", "password123")
            || HotspotManager::validConfiguration("", "password123")
            || HotspotManager::validConfiguration("test", "short")
            || HotspotManager::validConfiguration(QString(33, 'x'), "password123")) return 8;
        clipboardOcr.finish(0, QStringLiteral("OCR UI 测试\n本地识别结果可编辑"), QStringLiteral("本地识别完成 · 测试数据"));
        islandBridge.publish("test.low", {{"title","low"}}, 1);
        islandBridge.publish("test.high", {{"title","high"},{"progress",2.0}}, 9);
        if (islandBridge.activeState().value("sourceId") != "test.high"
            || islandBridge.activeState().value("progress").toDouble() != 1.0
            || islandBridge.publish("bad/id",{},0)) return 6;
        islandBridge.withdraw("test.high");
        if (islandBridge.activeState().value("sourceId") != "test.low") return 6;
        islandBridge.withdraw("test.low");
        QSize iconSize;
        const auto icon = fluentProvider->requestImage("desktop/61a9ff", &iconSize, QSize(40, 40));
        if (icon.isNull() || iconSize != QSize(40, 40)) {
            qCritical() << "UI_SELF_TEST Fluent SVG resource failed";
            return 5;
        }
    }

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        [](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) {
                qDebug() << warning.toString();
            }
        });

    engine.loadFromModule("qt_rainmeter", "Main");
    return app.exec();
}
