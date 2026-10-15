#include "include/uicardmanager.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <limits>
#include <iostream>
#define CHECK(v) do { if(!(v)) { std::cerr<<"FAIL "<<__LINE__<<": "<<#v<<'\n'; return 1; } } while(false)
static QVariantMap card(const QString &id,const QString &page="monitor") {
    return {{"id",id},{"page",page},{"title","Fixture"},{"minimumWidth",200},{"minimumHeight",96},{"defaultHeight",144},{"maximumHeight",640}};
}
static QRectF rect(const QVariantMap &g) { return {g.value("x").toDouble(),g.value("y").toDouble(),g.value("width").toDouble(),g.value("height").toDouble()}; }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); QTemporaryDir temp; CHECK(temp.isValid()); const auto path=temp.filePath("cards/layout.json");
    UiCardManager manager(nullptr,path); CHECK(manager.registerCard(card("first"))); CHECK(manager.registerCard(card("second")));
    CHECK(manager.registerCard(card("focus","focus"))); CHECK(!manager.registerCard(card("first")));
    CHECK(manager.metaObject()->indexOfProperty("dataRevision")==-1);
    CHECK(manager.metaObject()->indexOfMethod("requestAction(QString,QString)")==-1);
    auto uiDefinition=card("bad.ui"); uiDefinition["componentPath"]="External.qml"; CHECK(!manager.registerCard(uiDefinition));
    auto actionDefinition=card("bad.action"); actionDefinition["actions"]=QStringList{"refresh"}; CHECK(!manager.registerCard(actionDefinition));
    auto bad=card("bad"); bad["content"]=QVariantMap{{"type","native"},{"component","../../External.qml"}}; CHECK(!manager.registerCard(bad));
    bad=card("bad"); bad["content"]=QVariantMap{{"type","text"},{"props",QVariantMap{{"text","x"},{"onClicked","eval"}}}}; CHECK(!manager.registerCard(bad));
    CHECK(!manager.setGeometry("first",0,0,100,100,680));
    const auto original=manager.geometry("first",680); CHECK(original.value("width").toDouble()==332);
    CHECK(manager.beginEdit("monitor",680)); CHECK(!manager.beginEdit("focus",680));
    CHECK(!manager.setGeometry("focus",0,0,300,150,680));
    CHECK(!manager.setGeometry("first",std::numeric_limits<double>::quiet_NaN(),0,300,150,680));
    CHECK(manager.setGeometry("first",347,0,300,199,680));
    auto moved=manager.geometry("first",680); CHECK(!rect(moved).intersects(rect(manager.geometry("second",680))));
    CHECK(moved.value("height").toInt()==200);
    CHECK(!QFile::exists(path)); manager.cancelEdit(); CHECK(manager.geometry("first",680)==original);
    CHECK(manager.beginEdit("monitor",680)); CHECK(manager.setGeometry("first",8,400,400,208,680)); CHECK(manager.saveEdit());
    CHECK(QFile::exists(path) && !manager.editing()); const auto saved=manager.geometry("first",680);
    { UiCardManager reopened(nullptr,path); CHECK(reopened.registerCard(card("first"))); CHECK(reopened.registerCard(card("second"))); CHECK(reopened.geometry("first",680)==saved); }
    CHECK(manager.geometry("first",360).value("y").toDouble()==0); // Independent narrow profile.
    CHECK(manager.beginEdit("monitor",680)); CHECK(manager.setVisible("first",false)); CHECK(!manager.geometry("first",680).value("visible").toBool());
    CHECK(manager.resetDraft()); CHECK(manager.geometry("first",680).value("visible").toBool()); manager.cancelEdit(); CHECK(manager.geometry("first",680)==saved);
    CHECK(manager.beginEdit("monitor",680)); CHECK(manager.setVisible("first",false)); CHECK(manager.saveEdit()); CHECK(!manager.geometry("first",680).value("visible").toBool());
    CHECK(manager.beginEdit("monitor",360));
    CHECK(manager.setGeometry("first",400,0,1000,10,360)); CHECK(rect(manager.geometry("first",360)).right()<=360);
    CHECK(manager.geometry("first",680).value("width").toDouble()<=680); // Width changes do not abandon edit transaction.
    manager.cancelEdit(); CHECK(manager.reportMinimumHeight("second",320)); CHECK(manager.geometry("second",680).value("height").toDouble()>=320);
    UiCardManager failed(nullptr,temp.path()); CHECK(failed.registerCard(card("first"))); CHECK(failed.beginEdit("monitor",360)); CHECK(!failed.saveEdit() && failed.editing());
    QFile corrupt(temp.filePath("corrupt.json")); CHECK(corrupt.open(QIODevice::WriteOnly)); corrupt.write("{broken"); corrupt.close();
    UiCardManager fallback(nullptr,corrupt.fileName()); CHECK(!fallback.lastError().isEmpty()); CHECK(fallback.registerCard(card("first"))); CHECK(!fallback.geometry("first",360).isEmpty());
    manager.setAvailable("second",false); CHECK(!manager.geometry("second",680).value("visible").toBool());
    CHECK(manager.geometry("second",680).value("userVisible").toBool());
    CHECK(!manager.geometry("second",680).value("available").toBool());
    manager.setAvailable("second",true); CHECK(manager.geometry("second",680).value("visible").toBool());
    CHECK(manager.beginEdit("monitor",680)); CHECK(manager.resetDraft()); CHECK(manager.geometry("first",680).value("y").toDouble()==0); manager.cancelEdit();
    std::cout<<"CARD MANAGER PASS: schema/catalog, grid/collisions/bounds, page ownership, drafts/cancel/reset/save/reopen, profiles, layout-only metadata, dynamic minimum, save failure, corruption fallback; temporary data only\n";
}
