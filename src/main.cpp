#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QMetaType>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <winrt/base.h>

#include "include/coverimageprovider.h"
#include "include/cpumonitor.h"
#include "include/diskmonitor.h"
#include "include/gpumonitor.h"
#include "include/mediamonitor.h"
#include "include/memorymonitor.h"
#include "include/monitorscheduler.h"
#include "include/networkmonitor.h"
#include "include/notificationlistener.h"
#include "include/notificationmodel.h"

int main(int argc, char *argv[])
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    QGuiApplication app(argc, argv);
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<Notification>("Notification");

    QQmlApplicationEngine engine;
    CpuMonitor cpuMonitor;
    GpuMonitor gpuMonitor;
    DiskMonitor diskMonitor;
    MemoryMonitor memoryMonitor;
    NetworkMonitor networkMonitor;
    MediaMonitor mediaMonitor;
    NotificationModel notificationModel;
    NotificationListener notificationListener;
    MonitorScheduler monitorScheduler;

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
    engine.rootContext()->setContextProperty("gpuMonitor", &gpuMonitor);
    engine.rootContext()->setContextProperty("diskMonitor", &diskMonitor);
    engine.rootContext()->setContextProperty("memoryMonitor", &memoryMonitor);
    engine.rootContext()->setContextProperty("networkMonitor", &networkMonitor);
    engine.rootContext()->setContextProperty("mediaMonitor", &mediaMonitor);
    engine.rootContext()->setContextProperty("notificationModel", &notificationModel);
    engine.addImageProvider("cover", coverProvider);

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
