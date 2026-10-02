#include "include/islandbridge.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QJSValue>
#include <QQuickImageProvider>
#include <QSvgRenderer>
#include <QPainter>
#include <QMouseEvent>
#include <QtTest/QTest>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
class Icons : public QQuickImageProvider {
    QString directory;
public:
    explicit Icons(QString path) : QQuickImageProvider(Image), directory(std::move(path)) {}
    QImage requestImage(const QString &id,QSize *size,const QSize &) override {
        const auto parts=id.split('/');
        QSvgRenderer svg(directory+'/'+parts.value(0)+"_20_regular.svg");
        if (!svg.isValid()) return {};
        QImage image(40,40,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        QPainter painter(&image); svg.render(&painter);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn); painter.fillRect(image.rect(),QColor('#'+parts.value(1)));
        if (size) *size=image.size(); return image;
    }
};
static void settle() { QEventLoop loop; QTimer::singleShot(350,&loop,&QEventLoop::quit); loop.exec(); }
int main(int argc,char **argv) {
    // This executable contains no app services, audio workers or device managers.
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &message) { std::cerr<<message.toStdString()<<'\n'; });
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_BACKEND","software");
    QGuiApplication app(argc,argv);
    if (argc!=3) return 1;
    IslandBridge bridge;
    QQmlPropertyMap media,audio,spectrum,theme;
    media.insert("coverVersion",0);
    audio.insert("inputMeterAvailable",false); audio.insert("muted",false); audio.insert("inputPeak",0.0);
    spectrum.insert("requested",false); spectrum.insert("enabled",false); spectrum.insert("available",false); spectrum.insert("bands",QVariantList{});
    theme.insert("textPrimary","#FFFFFF"); theme.insert("textSecondary","#B5B5BE");
    QQuickView view;
    auto *context=view.rootContext();
    context->setContextProperty("islandBridge",&bridge); context->setContextProperty("mediaMonitor",&media);
    context->setContextProperty("audioInputManager",&audio); context->setContextProperty("mediaSpectrum",&spectrum); context->setContextProperty("themeManager",&theme);
    view.engine()->addImageProvider("fluent",new Icons(QString::fromLocal8Bit(argv[1])+"/../assets/fluent"));
    int warnings=0;
    QObject::connect(view.engine(),&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &errors) { for (const auto &error:errors) std::cerr<<error.toString().toStdString()<<'\n'; warnings+=errors.size(); });
    view.setResizeMode(QQuickView::SizeViewToRootObject);
    view.setColor(Qt::transparent);
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])+"/TopIslandHost.qml"));
    if (!view.rootObject()) return 2;
    view.show();
    QImage sheet(460,6*88,QImage::Format_ARGB32_Premultiplied); sheet.fill(QColor("#17202B"));
    int row=0;
    for (const auto &kind:QStringList{"awake","ocr","usb","network","focus","notification"}) {
        QVariantMap state{{"sourceId","fixture"},{"kind",kind},{"mode","event"},{"title",kind=="usb" ? "USB 安全弹出未完成" : kind=="awake" ? "保持唤醒已到期" : kind=="ocr" ? "图片文字识别完成" : kind=="network" ? "热点操作已结束" : kind=="focus" ? "这一轮专注完成了" : "新通知"},
            {"subtitle","点击查看详情 · 关闭不取消任务"},{"tone",kind=="usb" ? "danger" : "success"},{"iconName",kind=="network" ? "wifi_1" : kind=="ocr" ? "clipboard" : "checkmark"},{"queuedCount",3},{"eventProgress",0.45}};
        view.rootObject()->setProperty("stateOverride",state); settle();
        const auto frame=view.grabWindow();
        if (frame.isNull() || view.rootObject()->width()!=420 || view.rootObject()->height()!=64) return 3;
        QPainter painter(&sheet); painter.drawImage(QPoint(20,row++*88+12),frame);
    }
    if (!sheet.save(QString::fromLocal8Bit(argv[2]))) return 4;
    view.rootObject()->setProperty("stateOverride",QVariant::fromValue(QJSValue(QJSValue::NullValue)));
    bridge.publishState("base",{{"title","持续活动"}},20);
    bridge.pushEvent("notice","close",{{"title","关闭测试"}},80,10000,20000); settle();
    int details=0;
    QObject::connect(&bridge,&IslandBridge::detailsRequested,&app,[&](const QString &) { ++details; });
    auto *button=view.rootObject()->findChild<QQuickItem *>("islandDismiss");
    if (!button || !button->isVisible() || !button->isEnabled()) return 6;
    const QPointF point=button->mapToScene(QPointF(button->width()/2,button->height()/2));
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,point.toPoint(),50); settle();
    if (bridge.eventActive() || details!=0 || bridge.activeState().value("sourceId")!="base" || warnings) {
        std::cerr<<"Dismiss check: event="<<bridge.eventActive()<<" details="<<details<<" source="<<bridge.activeState().value("sourceId").toString().toStdString()<<" warnings="<<warnings<<'\n';
        std::cerr<<"Button="<<point.x()<<","<<point.y()<<" size="<<button->width()<<","<<button->height()<<" view="<<view.width()<<","<<view.height()<<'\n';
        return 5;
    }
    std::cout<<"ISLAND UI PASS: 6 rendered cases + dismiss does not trigger details + zero QML warnings; mock-only offscreen\n";
}
