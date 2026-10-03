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
#include <QSignalSpy>
#include <QStyleHints>
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
static QQuickItem *findVisual(QQuickItem *item, const QString &name) {
    if (item->objectName()==name) return item;
    for (auto *child:item->childItems()) if (auto *found=findVisual(child,name)) return found;
    return nullptr;
}
class Backdrop : public QQuickItem {
public:
    int presses=0;
    explicit Backdrop(QQuickItem *parent):QQuickItem(parent) { setAcceptedMouseButtons(Qt::AllButtons); }
protected:
    void mousePressEvent(QMouseEvent *event) override { ++presses; event->accept(); }
};
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
    // Live updates are not presentation overrides: exercise actual animated
    // geometry, interrupt expansion, and ensure a clock tick does not re-enter.
    bridge.withdraw("base"); settle();
    if (view.rootObject()->width()!=64 || view.rootObject()->height()!=24) return 7;
    bridge.publishState("motion",{{"title","专注"},{"kind","focus"},{"detailText","1:00"}},20);
    QTest::qWait(60);
    const qreal intermediate = view.rootObject()->width();
    if (intermediate<=64 || intermediate>=344 || !view.rootObject()->property("motionRunning").toBool()) return 8;
    bridge.pushEvent("alert","motion",{{"title","高优先级提醒"},{"kind","usb"}},95,10000,20000);
    settle();
    if (view.rootObject()->width()!=420 || view.rootObject()->height()!=64) return 9;
    bridge.dismissCurrentEvent(); settle();
    const auto key = view.rootObject()->property("contentKey");
    bridge.publishState("motion",{{"title","专注"},{"kind","focus"},{"detailText","0:59"}},20);
    if (view.rootObject()->property("contentKey")!=key || view.rootObject()->property("motionRunning").toBool()) return 10;
    view.rootObject()->setProperty("motionEnabled",false);
    bridge.withdraw("motion");
    if (view.rootObject()->width()!=64 || view.rootObject()->height()!=24) return 11;
    view.rootObject()->setProperty("motionEnabled",true);
    view.rootObject()->setProperty("availableWidth",280);
    bridge.publishState("narrow",{{"title","窄屏"},{"kind","media"}},40); settle();
    if (view.rootObject()->width()!=280 || warnings) return 12;
    view.rootObject()->setProperty("availableWidth",420);
    view.rootObject()->setProperty("controlData",QVariantMap{{"status","当前任务快捷控制"},
        {"targetId","session-a"},{"actions",QVariantList{QVariantMap{{"id","media.toggle"},{"label","暂停"},{"enabled",true}},
        QVariantMap{{"id","media.next"},{"label","下一首"},{"enabled",false}}}}});
    view.rootObject()->setProperty("controlsOpen",true); settle();
    if (view.rootObject()->width()!=420 || view.rootObject()->height()<=100) return 13;
    auto *action=findVisual(view.rootObject(),"islandAction_media.toggle");
    auto *disabled=findVisual(view.rootObject(),"islandAction_media.next");
    if (!action || !disabled || disabled->isEnabled()) return 14;
    int actions=0;
    // QSignalSpy verifies the data-only boundary without launching a media worker.
    QSignalSpy actionSpy(view.rootObject(), SIGNAL(actionRequested(QString,QString,QString)));
    const auto actionPoint=action->mapToScene(QPointF(action->width()/2,action->height()/2));
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,actionPoint.toPoint(),50); settle();
    actions=actionSpy.count();
    if (actions!=1 || actionSpy.first().at(0)!="narrow" || actionSpy.first().at(1)!="media.toggle" || actionSpy.first().at(2)!="session-a" || details!=0) return 15;
    auto *detailsButton=view.rootObject()->findChild<QQuickItem *>("islandDetails");
    if (!detailsButton) return 16;
    const auto detailsPoint=detailsButton->mapToScene(QPointF(detailsButton->width()/2,detailsButton->height()/2));
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,detailsPoint.toPoint(),50); settle();
    if (details!=1 || view.rootObject()->property("controlsOpen").toBool() || warnings) return 17;
    view.rootObject()->setProperty("controlsOpen",true); settle();
    if (!view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".expanded.png")) return 18;
    Backdrop backdrop(view.contentItem());
    backdrop.setZ(-1); backdrop.setWidth(460); backdrop.setHeight(400);
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,QPoint(400,int(view.rootObject()->height())-6),50); settle();
    if (backdrop.presses!=0) return 19;
    view.rootObject()->setProperty("controlsOpen",false); settle();
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,QPoint(150,26),50);
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval()+80);
    if (!view.rootObject()->property("controlsOpen").toBool() || details!=1) return 20;
    bridge.publishState("preempt",{{"title","麦克风"},{"kind","input"}},60); settle();
    if (view.rootObject()->property("controlsOpen").toBool()) return 21;
    std::cout<<"ISLAND UI PASS: 6 rendered cases + dismiss does not trigger details + zero QML warnings; mock-only offscreen\n";
    std::cout<<"ISLAND MOTION PASS: intermediate geometry, interrupted target, stable clock updates, instant mode, narrow bounds\n";
    std::cout<<"ISLAND CONTROLS PASS: expansion, disabled capability, scoped action signal, explicit source navigation, no background click-through, single tap, preemption closes controls\n";
}
