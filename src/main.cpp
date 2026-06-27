#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QMetaType>
#include <QImage>
#include <winrt/base.h>
#include "cpumonitor.h"
#include "MemoryMonitor.h"
#include "networkmonitor.h"
#include "mediamonitor.h"
#include "coverimageprovider.h"
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    qRegisterMetaType<QImage>("QImage");
    QQmlApplicationEngine engine;

    CpuMonitor cpuMonitor;
    MemoryMonitor memoryMonitor;
    NetworkMonitor networkMonitor;
    MediaMonitor mediaMonitor;
    auto coverProvider=new CoverImageProvider;
    mediaMonitor.setCoverProvider(coverProvider);
    engine.rootContext()->setContextProperty("cpuMonitor",&cpuMonitor);
    engine.rootContext()->setContextProperty("memoryMonitor",&memoryMonitor);
    engine.rootContext()->setContextProperty("networkMonitor",&networkMonitor);
    engine.rootContext()->setContextProperty("mediaMonitor",&mediaMonitor);
    engine.addImageProvider("cover",coverProvider);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        [](const QList<QQmlError>& warnings)
        {
            for (const auto& w : warnings)
            {
                qDebug() << w.toString();
            }
        });
    engine.loadFromModule("qt_rainmeter", "Main");

    return app.exec();
}
