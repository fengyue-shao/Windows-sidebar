#include "include/uicardmanager.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QtTest/QTest>
#include <iostream>
#define CHECK(v) do { if(!(v)) { std::cerr<<"FAIL "<<__LINE__<<": "<<#v<<'\n'; return 1; } } while(false)
static QQuickItem *find(QQuickItem *r,const QString &name) { if(r->objectName()==name) return r; for(auto *c:r->childItems()) if(auto *v=find(c,name)) return v; return nullptr; }
static void move(QQuickView &view,const QPoint &from,const QPoint &to) {
    QTest::mousePress(&view,Qt::LeftButton,Qt::NoModifier,from);
    QMouseEvent event(QEvent::MouseMove,QPointF(to),QPointF(view.mapToGlobal(to)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&event); QTest::qWait(40);
    QTest::mouseRelease(&view,Qt::LeftButton,Qt::NoModifier,to); QTest::qWait(100);
}
int main(int argc,char **argv) {
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_BACKEND","software"); QGuiApplication app(argc,argv); if(argc<3) return 2;
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf"); QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QTemporaryDir temp; CHECK(temp.isValid()); UiCardManager manager(nullptr,temp.filePath("layout.json"));
    QQmlPropertyMap theme; for(const auto &key:{"cardSurface","border","accentSoft","textSecondary","textPrimary","accent"}) theme.insert(key,QString(key).startsWith("text") ? "#EDF2FA" : QString(key)=="accent" ? "#72A7FF" : "#202A38");
    QQuickView view; view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(1000,700); view.setColor(QColor("#10151D")); view.rootContext()->setContextProperty("themeManager",&theme); view.setInitialProperties({{"manager",QVariant::fromValue(&manager)}});
    int warnings=0; QObject::connect(view.engine(),&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &errors){warnings+=errors.size(); for(const auto &e:errors) std::cerr<<e.toString().toStdString()<<'\n';});
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1]))); CHECK(view.rootObject()); view.show(); QTest::qWait(250);
    auto click=[&](const QString &name) { auto *item=find(view.rootObject(),name); if(!item || !item->isVisible()) return false; QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint()); QTest::qWait(100); return true; };
    CHECK(click("desktopLayoutEdit")); CHECK(manager.editing());
    auto *frame=find(view.rootObject(),"desktopCard-first"); CHECK(frame);
    auto *drag=find(view.rootObject(),"desktopCardDrag-first"); CHECK(drag);
    const auto p=drag->mapToScene(QPointF(20,16)).toPoint(); move(view,p,p+QPoint(13,410));
    CHECK(qAbs(manager.desktopGeometry("first").value("x").toDouble()-13)<0.01); // Not snapped to 8px.
    auto *resize=find(view.rootObject(),"desktopCardResize-first"); CHECK(resize); const auto q=resize->mapToScene(QPointF(12,12)).toPoint(); const auto w=frame->width(); move(view,q,q+QPoint(47,31)); CHECK(qAbs(frame->width()-w-47)<0.01);
    const auto beforeCollision=manager.desktopGeometry("first"); const auto other=manager.desktopGeometry("second");
    const auto start=drag->mapToScene(QPointF(20,16)).toPoint();
    move(view,start,QPoint(qRound(other.value("x").toDouble()+20),qRound(other.value("y").toDouble()+16)));
    CHECK(manager.desktopGeometry("first")==beforeCollision); // Invalid mouse move never stacks even temporarily.
    auto *business=find(frame,"desktopBusiness"); CHECK(business); QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,business->mapToScene(QPointF(20,10)).toPoint()); QTest::qWait(40); CHECK(view.rootObject()->property("actions").toInt()==0);
    CHECK(click("desktopCardPinned-first")); CHECK(manager.hasPinnedCards());
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".editing.png"));
    CHECK(click("desktopCardEnabled-second")); CHECK(!manager.desktopGeometry("second").value("enabled").toBool());
    CHECK(click("desktopLayoutCards")); CHECK(click("desktopRestore-second")); CHECK(manager.desktopGeometry("second").value("enabled").toBool());
    CHECK(click("desktopLayoutCards")); CHECK(click("desktopLayoutSave")); CHECK(!manager.editing());
    const auto location=QPointF(frame->x(),frame->y());
    CHECK(view.rootObject()->setProperty("expanded",false)); QTest::qWait(100);
    CHECK(frame->isVisible()); CHECK(QPointF(frame->x(),frame->y())==location);
    CHECK(!find(view.rootObject(),"desktopCard-second")->isVisible());
    CHECK(!find(frame,"desktopCardHeader-first")->isVisible());
    CHECK(!find(view.rootObject(),"desktopLayoutEdit")->isVisible());
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".pinned.png"));
    business=find(frame,"desktopBusiness"); CHECK(business); QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,business->mapToScene(QPointF(20,10)).toPoint()); QTest::qWait(40); CHECK(view.rootObject()->property("actions").toInt()==1);
    view.resize(360,640); QTest::qWait(150); CHECK(frame->x()>=0 && frame->y()>=0 && frame->x()+frame->width()<=360 && frame->y()+frame->height()<=640);
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".narrow.png")); CHECK(warnings==0);
    std::cout<<"DESKTOP CARD CANVAS PASS: actual free drag/resize, editing input guard, header toggles, close/restore, pinned position stable after collapse, business input when pinned, narrow bounds, zero QML warnings\n";
}
