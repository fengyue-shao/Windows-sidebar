#include "include/islandbridge.h"
#include "include/captureusagemonitor.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    IslandBridge island;
    island.publishState("presentation",{{"kind","media"},{"tone","danger"},{"detailText",QString(60,'x')},{"applicationCount",2000},{"progress",qQNaN()}},1);
    const auto presentation=island.activeState();
    if (presentation.value("kind")!="media" || presentation.value("tone")!="danger"
        || presentation.value("detailText").toString().size()!=24 || presentation.value("applicationCount").toInt()!=999
        || presentation.contains("progress")) return 9;
    island.publishState("presentation",{{"kind","script"},{"tone","arbitrary"}},1);
    if (island.activeState().contains("kind") || island.activeState().contains("tone")) return 10;
    island.withdraw("presentation");
    int wakeCount=0;
    QObject::connect(&island,&IslandBridge::displayRequested,&app,[&] { ++wakeCount; });
    island.publishState("focus",{{"title","focus"}},20);
    island.publishState("media",{{"title","media"}},40);
    island.publishState("input",{{"title","input"}},60);
    if (island.activeState().value("title")!="input") return 2;
    const int wakes=wakeCount;
    for (int i=0;i<100;++i) island.publishState("input",{{"title","input"},{"progress",double(i)/100}},60);
    if (wakeCount!=wakes) return 3;
    island.withdraw("input");
    island.publishState("equal",{{"title","equal"}},40);
    if (island.activeState().value("title")!="media") return 4;
    island.pushEvent("event","a",{{"title","A"}},80,1000,5000);
    island.pushEvent("event","b",{{"title","B"}},90,1000,5000);
    island.pushEvent("event","b",{{"title","B merged"}},90,1000,5000);
    island.pushEvent("event","low",{{"title","expired"}},10,1000,1000);
    if (island.activeState().value("title")!="A" || island.queuedCount()!=3) return 5;
    CaptureUsageMonitor monitor;
    QTimer::singleShot(1300,&app,[&] {
        if (island.activeState().value("title")!="B merged") { qCritical()<<"ISLAND queue order failed"; app.exit(6); }
    });
    QTimer::singleShot(2600,&app,[&] {
        if (island.activeState().value("title")!="media" || island.queuedCount()!=0 || island.pushEvent("event","a",{},80)) { app.exit(7); return; }
        if (!monitor.available() || monitor.observedEndpointCount()==0 || monitor.statusText().contains(QStringLiteral("部分"))) { qCritical()<<"CAPTURE observer unavailable"<<monitor.statusText(); app.exit(8); return; }
        qInfo()<<"ISLAND PASS unique priority + stable updates + FIFO buffering + coalesce + expiry + restore + dedup + wake signal";
        qInfo()<<"CAPTURE OBSERVER PASS endpoint enumeration + session subscription, endpoints="<<monitor.observedEndpointCount()<<"active application count="<<monitor.applications().size()<<"(no microphone stream opened)";
        app.quit();
    });
    return app.exec();
}
