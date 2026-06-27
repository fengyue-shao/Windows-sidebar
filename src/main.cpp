#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "cpumonitor.h"
#include "MemoryMonitor.h"
#include "networkmonitor.h"
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    CpuMonitor cpuMonitor;
    MemoryMonitor memoryMonitor;
    NetworkMonitor networkMonitor;
    engine.rootContext()->setContextProperty("cpuMonitor",&cpuMonitor);
    engine.rootContext()->setContextProperty("memoryMonitor",&memoryMonitor);
    engine.rootContext()->setContextProperty("networkMonitor",&networkMonitor);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("qt_rainmeter", "Main");

    return app.exec();
}
