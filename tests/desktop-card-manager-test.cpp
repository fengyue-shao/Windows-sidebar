#include "include/uicardmanager.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <iostream>
#include <limits>
#define CHECK(v) do { if(!(v)) { std::cerr<<"FAIL "<<__LINE__<<": "<<#v<<'\n'; return 1; } } while(false)
static QVariantMap card(const QString &id) { return {{"id",id},{"page","monitor"},{"title",id},{"minimumWidth",160},{"minimumHeight",96},{"defaultHeight",144},{"maximumHeight",2048}}; }
static QRectF rect(const QVariantMap &g) { return {g.value("x").toDouble(),g.value("y").toDouble(),g.value("width").toDouble(),g.value("height").toDouble()}; }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); QTemporaryDir temp; CHECK(temp.isValid()); const auto path=temp.filePath("layout.json");
    UiCardManager m(nullptr,path); CHECK(m.registerCard(card("first"))); CHECK(m.registerCard(card("second")));
    m.configureDesktop(1200,800,{{QVariantMap{{"x",400},{"y",0},{"width",400},{"height",160}}}});
    CHECK(!m.hasPinnedCards()); CHECK(!m.setPinned("first",true));
    CHECK(m.beginDesktopEdit("monitor"));
    CHECK(!m.setVisible("first",false)); // Old profile API cannot mutate screen draft.
    CHECK(m.setDesktopGeometry("first",23.5,350.25,277.5,163.25)); // No grid rounding; outside second card.
    CHECK(rect(m.desktopGeometry("first"))==QRectF(23.5,350.25,277.5,163.25));
    CHECK(!m.setDesktopGeometry("first",-1,0,240,144));
    CHECK(!m.setDesktopGeometry("first",1190,700,240,144));
    CHECK(!m.setDesktopGeometry("first",420,0,240,144));
    CHECK(!m.setDesktopGeometry("first",0,0,std::numeric_limits<double>::quiet_NaN(),144));
    const auto second=rect(m.desktopGeometry("second"));
    CHECK(!m.setDesktopGeometry("first",second.x(),second.y(),second.width(),second.height()));
    CHECK(m.setPinned("first",true)); CHECK(m.hasPinnedCards());
    CHECK(m.setEnabled("first",false)); CHECK(!m.hasPinnedCards());
    CHECK(m.desktopGeometry("first").value("pinned").toBool());
    CHECK(m.setEnabled("first",true)); CHECK(m.hasPinnedCards());
    m.setAvailable("first",false); CHECK(!m.hasPinnedCards()); CHECK(m.desktopGeometry("first").value("enabled").toBool());
    m.setAvailable("first",true); CHECK(m.hasPinnedCards());
    CHECK(m.saveEdit()); CHECK(!m.editing());
    const auto saved=m.desktopGeometry("first");
    { UiCardManager reopened(nullptr,path); CHECK(reopened.lastError().isEmpty()); CHECK(reopened.registerCard(card("first"))); CHECK(reopened.registerCard(card("second"))); reopened.configureDesktop(1200,800,{{QVariantMap{{"x",400},{"y",0},{"width",400},{"height",160}}}}); CHECK(reopened.desktopGeometry("first")==saved); CHECK(reopened.hasPinnedCards()); }
    CHECK(m.beginDesktopEdit("monitor")); CHECK(m.setPinned("first",false)); CHECK(m.setEnabled("second",false)); m.cancelEdit(); CHECK(m.desktopGeometry("first")==saved); CHECK(m.desktopGeometry("second").value("enabled").toBool());
    m.configureDesktop(360,240,{});
    QList<QRectF> occupied;
    for(const auto &id:{"first","second"}) { const auto g=m.desktopGeometry(id); if(g.value("visible").toBool()) { CHECK(QRectF(0,0,360,240).contains(rect(g))); for(const auto &r:occupied) CHECK(!r.intersects(rect(g))); occupied.append(rect(g)); } }
    CHECK(m.beginDesktopEdit("monitor")); CHECK(m.resetDraft()); CHECK(m.saveEdit()); // Capacity overflow never serializes invalid zero dimensions.
    UiCardManager failed(nullptr,temp.path()); CHECK(failed.registerCard(card("first"))); failed.configureDesktop(640,480,{}); CHECK(failed.beginDesktopEdit("monitor")); CHECK(!failed.saveEdit()); CHECK(failed.editing());
    const auto legacy=temp.filePath("legacy.json"); QFile legacyFile(legacy); CHECK(legacyFile.open(QIODevice::WriteOnly)); legacyFile.write("{\"schemaVersion\":1,\"layouts\":{}}"); legacyFile.close();
    UiCardManager migration(nullptr,legacy); CHECK(migration.lastError().isEmpty()); CHECK(migration.registerCard(card("first"))); migration.configureDesktop(640,480,{}); CHECK(migration.beginDesktopEdit("monitor")); CHECK(migration.setPinned("first",true)); CHECK(migration.saveEdit());
    QFile upgraded(legacy); CHECK(upgraded.open(QIODevice::ReadOnly)); CHECK(QJsonDocument::fromJson(upgraded.readAll()).object().value("schemaVersion")==2);
    std::cout<<"DESKTOP CARD MANAGER PASS: free coordinates, screen bounds, collision/reserved rejection, enabled/pinned, cancel/save/reopen, small-screen safe overflow, legacy schema upgrade, save failure; temporary data only\n";
}
