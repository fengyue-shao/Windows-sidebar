#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QFontDatabase>
#include <QtTest/QTest>
#include <iostream>
#define CHECK(v) do { if (!(v)) { std::cerr << "FAIL " << __LINE__ << ": " << #v << '\n'; return 1; } } while(false)
static QQuickItem *findItem(QQuickItem *root,const QString &name) {
    if (root->objectName()==name) return root;
    for (auto *child:root->childItems()) if (auto *match=findItem(child,name)) return match;
    return nullptr;
}
class RegistryFixture : public QQmlPropertyMap {
    Q_OBJECT
public:
    RegistryFixture():QQmlPropertyMap(this,nullptr){}
    int selections=0;
    QString chosen;
    bool confirmed=false;
    Q_INVOKABLE bool selectVersion(const QString &,const QString &version,bool confirm=false){++selections; chosen=version; confirmed=confirm; return true;}
    Q_INVOKABLE void reload(){}
    Q_INVOKABLE void launch(const QString &){}
    Q_INVOKABLE void stop(const QString &){}
    Q_INVOKABLE void setPluginEnabled(const QString &,bool){}
    Q_INVOKABLE void trustAndEnable(const QString &){}
    Q_INVOKABLE bool importTrustedFolder(const QString &){return true;}
    Q_INVOKABLE void openPluginFolder(){}
};
int main(int argc,char **argv) {
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_BACKEND","software");
    QGuiApplication app(argc,argv); if (argc!=3) return 2;
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    RegistryFixture registry; QQmlPropertyMap theme;
    for (const auto &key:{"surface","surfaceStrong","surfaceHover","cardSurface","overlayStart","border","textPrimary","textSecondary","textMuted","textFaint","accent","accentSoft","danger"})
        theme.insert(key,QString(key).contains("text") ? "#E8EEF8" : QString(key)=="accent" ? "#72A7FF" : "#202A38");
    QVariantMap descriptor{{"id","test.ui"},{"name","受限终端"},{"version","1.0.0"},{"enabled",true},{"trusted",true},{"pageCount",1},{"running",true},
        {"processStatus","running"},{"pendingVersion",""},{"permissionsText","受信任的模拟插件，不运行系统命令"},
        {"versions",QVariantList{QVariantMap{{"version","2.0.0"},{"trusted",true}},QVariantMap{{"version","1.0.0"},{"trusted",true}}}}};
    registry.insert("plugins",QVariantList{descriptor}); registry.insert("statusText","发现 1 个插件，2 个版本"); registry.insert("errorDetails","");
    QQuickView view; view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(620,780);
    view.rootContext()->setContextProperty("themeManager",&theme); view.rootContext()->setContextProperty("pluginRegistry",&registry);
    int warnings=0; QObject::connect(view.engine(),&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &errors){warnings+=errors.size(); for (const auto &error:errors) std::cerr<<error.toString().toStdString()<<'\n';});
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1]))); CHECK(view.rootObject()); view.show(); QTest::qWait(200);
    auto *choice=findItem(view.rootObject(),"pluginVersionChoice"); CHECK(choice && choice->property("currentIndex")==1);
    auto *versionPopup=choice->property("popup").value<QObject *>(); CHECK(versionPopup);
    CHECK(QMetaObject::invokeMethod(versionPopup,"open")); QTest::qWait(100);
    CHECK(view.rootObject()->property("popupOpen").toBool());
    CHECK(QMetaObject::invokeMethod(view.rootObject(),"closePopups")); QTest::qWait(100);
    CHECK(!view.rootObject()->property("popupOpen").toBool());
    CHECK(QMetaObject::invokeMethod(versionPopup,"open")); QTest::qWait(200);
    auto *popupContent=qobject_cast<QQuickItem *>(versionPopup->property("contentItem").value<QObject *>()); CHECK(popupContent);
    auto *option=findItem(popupContent,"pluginVersionOption-2.0.0"); CHECK(option);
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,option->mapToScene(QPointF(option->width()/2,option->height()/2)).toPoint()); QTest::qWait(150);
    auto *dialog=view.rootObject()->findChild<QObject *>("pluginVersionConfirmation"); CHECK(dialog);
    CHECK(dialog->property("visible").toBool() && registry.selections==0);
    CHECK(QMetaObject::invokeMethod(dialog,"reject")); QTest::qWait(100);
    CHECK(choice->setProperty("currentIndex",0)); CHECK(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,0))); QTest::qWait(100);
    CHECK(registry.selections==0 && view.rootObject()->property("popupOpen").toBool());
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".confirmation.png"));
    CHECK(QMetaObject::invokeMethod(dialog,"reject")); QTest::qWait(50); CHECK(registry.selections==0 && !view.rootObject()->property("popupOpen").toBool());
    CHECK(choice->setProperty("currentIndex",0)); CHECK(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,0))); QTest::qWait(50);
    CHECK(QMetaObject::invokeMethod(dialog,"accept")); CHECK(registry.selections==1 && registry.confirmed && registry.chosen=="2.0.0");
    CHECK(QMetaObject::invokeMethod(view.rootObject(),"closePopups")); QTest::qWait(100);
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])));
    view.resize(360,780); QTest::qWait(100);
    CHECK(view.grabWindow().save(QString::fromLocal8Bit(argv[2])+".narrow.png"));
    CHECK(warnings==0);
    std::cout<<"TOOLBOX UI PASS: production version selector, cancel preserves choice, confirmation dispatch, popup guard, render; fixture only\n";
    return 0;
}
#include "toolbox-ui-test.moc"
