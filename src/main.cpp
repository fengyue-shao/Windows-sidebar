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
#include "notificationmodel.h"
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    qRegisterMetaType<QImage>("QImage");
    QQmlApplicationEngine engine;

    CpuMonitor cpuMonitor;
    MemoryMonitor memoryMonitor;
    NetworkMonitor networkMonitor;
    MediaMonitor mediaMonitor;
    NotificationModel notificationModel;
    Notification spotify;
    spotify.appName = "Spotify";
    spotify.title = "正在播放";
    spotify.content = "The Weeknd - Blinding Lights";
    spotify.timestamp = QDateTime::currentDateTime().addSecs(-30);
    notificationModel.addNotification(spotify);

    Notification qq;
    qq.appName = "QQ";
    qq.title = "张三";
    qq.content = "今晚一起吃饭吗？";
    qq.timestamp = QDateTime::currentDateTime().addSecs(-300);
    notificationModel.addNotification(qq);

    Notification wechat;
    wechat.appName = "微信";
    wechat.title = "李四";
    wechat.content = "文件已发送";
    wechat.timestamp = QDateTime::currentDateTime().addSecs(-1800);
    notificationModel.addNotification(wechat);
    spotify.appName = "Spotify";
    spotify.title = "正在播放";
    spotify.content = "The Weeknd - Blinding Lights";
    spotify.timestamp = QDateTime::currentDateTime().addSecs(-30);
    notificationModel.addNotification(spotify);

    qq.appName = "QQ";
    qq.title = "张三";
    qq.content = "今晚一起吃饭吗？";
    qq.timestamp = QDateTime::currentDateTime().addSecs(-300);
    notificationModel.addNotification(qq);

    wechat.appName = "微信";
    wechat.title = "李四";
    wechat.content = "文件已发送";
    wechat.timestamp = QDateTime::currentDateTime().addSecs(-1800);
    notificationModel.addNotification(wechat);
    auto coverProvider=new CoverImageProvider;
    mediaMonitor.setCoverProvider(coverProvider);
    engine.rootContext()->setContextProperty("cpuMonitor",&cpuMonitor);
    engine.rootContext()->setContextProperty("memoryMonitor",&memoryMonitor);
    engine.rootContext()->setContextProperty("networkMonitor",&networkMonitor);
    engine.rootContext()->setContextProperty("mediaMonitor",&mediaMonitor);
    engine.addImageProvider("cover",coverProvider);
    engine.rootContext()->setContextProperty(
        "notificationModel",
        &notificationModel);
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
