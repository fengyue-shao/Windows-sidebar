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
#include "include/bluetoothmanager.h"
#include "include/clipboardmanager.h"
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
#include "include/radiomanager.h"
#include "include/pluginregistry.h"
#include "include/terminalmanager.h"
#include "include/wifimanager.h"
#include "include/notificationlistener.h"
#include "include/notificationmodel.h"
#include "include/systemtraymanager.h"
#include "include/thememanager.h"
#include "include/uisettings.h"

int main(int argc, char *argv[])
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion("0.8.1.0-preview");
    QCoreApplication::setOrganizationName("WindowsSidebar");
    QCoreApplication::setApplicationName("WindowsSidebar");
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<Notification>("Notification");

    ClipboardManager clipboardManager;
    BluetoothManager bluetoothManager;
    CpuMonitor cpuMonitor;
    GpuMonitor gpuMonitor;
    DiskMonitor diskMonitor;
    DisplayManager displayManager;
    FocusTimer focusTimer;
    MemoryMonitor memoryMonitor;
    NetworkMonitor networkMonitor;
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
    engine.rootContext()->setContextProperty("bluetoothManager", &bluetoothManager);
    engine.rootContext()->setContextProperty("gpuMonitor", &gpuMonitor);
    engine.rootContext()->setContextProperty("diskMonitor", &diskMonitor);
    engine.rootContext()->setContextProperty("displayManager", &displayManager);
    engine.rootContext()->setContextProperty("focusTimer", &focusTimer);
    engine.rootContext()->setContextProperty("memoryMonitor", &memoryMonitor);
    engine.rootContext()->setContextProperty("networkMonitor", &networkMonitor);
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
