#include "include/uicardmanager.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQmlEngine>
#include <QQmlDebuggingEnabler>
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QtTest/QTest>
#include <iostream>
#define CHECK(v) do { if(!(v)) { std::cerr<<"FAIL "<<__LINE__<<": "<<#v<<'\n'; return 1; } } while(false)
static QQuickItem *find(QQuickItem *r,const QString &name) { if(r->objectName()==name) return r; for(auto *c:r->childItems()) if(auto *v=find(c,name)) return v; return nullptr; }
static void move(QQuickView &view,const QPoint &from,const QPoint &to) {
    QTest::mousePress(&view,Qt::LeftButton,Qt::NoModifier,from);
    QMouseEvent event(QEvent::MouseMove,QPointF(to),QPointF(view.mapToGlobal(to)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&event); QTest::qWait(50);
    QTest::mouseRelease(&view,Qt::LeftButton,Qt::NoModifier,to); QTest::qWait(100);
}
int main(int argc,char **argv) {
    std::cerr<<"CARD CANVAS: entering GUI initialization\n";
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_BACKEND","software");
    QGuiApplication app(argc,argv); if(argc<3) return 2;
    std::cerr<<"CARD CANVAS: GUI initialized\n";
    // Explicitly opt-in, test executable only. Shipping application has no debug listener.
    if(qEnvironmentVariableIntValue("SIDEBAR_QML_PROFILE")==1) {
        QQmlDebuggingEnabler::setServices(QQmlDebuggingEnabler::profilerServices());
        QQmlDebuggingEnabler::enableDebugging(false);
    }
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf"); QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QTemporaryDir temp; CHECK(temp.isValid()); UiCardManager manager(nullptr,temp.filePath("layout.json"));
    QQmlPropertyMap theme; for(const auto &key:{"cardSurface","border","accentSoft","textSecondary","textPrimary","textMuted","accent","danger"}) theme.insert(key,QString(key).startsWith("text") ? "#EDF2FA" : QString(key)=="accent" ? "#72A7FF" : "#202A38");
    QQuickView view; view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(680,780); view.setColor(QColor("#10151D"));
    view.rootContext()->setContextProperty("themeManager",&theme);
    view.setInitialProperties({{"manager",QVariant::fromValue(&manager)}});
    int warnings=0; QObject::connect(view.engine(),&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &errors){warnings+=errors.size(); for(const auto &error:errors) std::cerr<<error.toString().toStdString()<<'\n';});
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1]))); CHECK(view.rootObject()); view.show(); QTest::qWait(250);
    auto *frame=find(view.rootObject(),"managedCard-first"); CHECK(frame && frame->height()>=180);
    auto actions=[&]{ return view.rootObject()->property("actions").toInt(); };
    const auto original=manager.geometry("first",680);
    auto *business=find(view.rootObject(),"managedAction-first-refresh"); CHECK(business);
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,business->mapToScene(QPointF(business->width()/2,business->height()/2)).toPoint()); QTest::qWait(50); CHECK(actions()==1); view.rootObject()->setProperty("actions",0);
    auto *edit=find(view.rootObject(),"cardLayoutEdit"); CHECK(edit);
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,edit->mapToScene(QPointF(edit->width()/2,edit->height()/2)).toPoint()); QTest::qWait(100); CHECK(manager.editing());
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,business->mapToScene(QPointF(business->width()/2,business->height()/2)).toPoint()); QTest::qWait(50); CHECK(actions()==0);
    auto *drag=find(view.rootObject(),"cardDrag-first"); CHECK(drag);
    const auto p=drag->mapToScene(QPointF(100,14)).toPoint(); move(view,p,p+QPoint(0,280));
    CHECK(manager.geometry("first",680).value("y").toDouble()>original.value("y").toDouble());
    auto *resize=find(view.rootObject(),"cardResize-first"); CHECK(resize);
    const auto q=resize->mapToScene(QPointF(12,12)).toPoint(); const auto height=frame->height(); move(view,q,q+QPoint(64,64));
    CHECK(frame->height()>height && actions()==0);
    CHECK(view.rootObject()->setProperty("metricValue","78%")); QTest::qWait(100); CHECK(find(view.rootObject(),"managedCard-first")==frame); CHECK(find(view.rootObject(),"managedAction-first-refresh")==business); CHECK(find(view.rootObject(),"fixtureMetric")->property("text").toString()=="78%");
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])));
    CHECK(manager.saveEdit()); QTest::qWait(100); CHECK(!manager.editing());
    CHECK(manager.beginEdit("monitor",680)); CHECK(manager.resetDraft()); manager.cancelEdit();
    CHECK(manager.beginEdit("monitor",680)); QTest::qWait(100);
    auto click=[&](const QString &name) {
        auto *item=find(view.rootObject(),name); if(!item) return false;
        QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
        QTest::qWait(100); return true;
    };
    CHECK(click("cardLayoutVisibility"));
    auto *panel=find(view.rootObject(),"cardVisibilityPanel"); CHECK(panel);
    auto *visibilityButton=find(view.rootObject(),"cardLayoutVisibility"); CHECK(visibilityButton);
    CHECK(panel->mapToScene(QPointF(0,0)).y() >= visibilityButton->mapToScene(QPointF(0,visibilityButton->height())).y());
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".selector.png"));
    CHECK(click("cardVisibility-first"));
    CHECK(!manager.geometry("first",680).value("visible").toBool());
    CHECK(!find(view.rootObject(),"managedAction-first-refresh")); // Hidden content is unloaded.
    CHECK(click("cardVisibility-first"));
    CHECK(manager.geometry("first",680).value("visible").toBool());
    CHECK(find(view.rootObject(),"managedAction-first-refresh"));
    manager.setAvailable("first",false); QTest::qWait(100);
    CHECK(find(view.rootObject(),"cardVisibility-first")->property("checked").toBool());
    CHECK(click("cardVisibility-first"));
    manager.setAvailable("first",true); QTest::qWait(100);
    CHECK(!manager.geometry("first",680).value("visible").toBool()); // Module toggle preserves preference.
    manager.cancelEdit(); QTest::qWait(100);
    CHECK(manager.geometry("first",680).value("visible").toBool());
    view.resize(360,780); QTest::qWait(200); CHECK(frame->width()<=360 && frame->x()>=0);
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".narrow.png"));
    CHECK(manager.beginEdit("monitor",360)); QTest::qWait(100);
    CHECK(click("cardLayoutVisibility"));
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".selector-narrow.png"));
    CHECK(click("cardLayoutCancel")); CHECK(!manager.editing());
    CHECK(warnings==0);
    std::cout<<"CARD CANVAS PASS: native QML content, actual mouse drag/resize, editing input guard, stable data updates, visibility selector/unload/restore, module preference isolation, save/cancel, narrow selector, zero QML warnings; no system services\n";
}
