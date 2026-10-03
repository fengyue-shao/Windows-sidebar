#include "include/islandbridge.h"
#include "include/awakemanager.h"
#include "include/focustimer.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>
#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL line " << __LINE__ << ": " << #condition << '\n'; return 1; } } while (false)
static void tick(QObject &object) { QMetaObject::invokeMethod(&object,"refresh",Qt::DirectConnection); }
int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    qint64 now=0;
    IslandBridge island(nullptr,[&] { return now; });
    int wakes=0;
    QObject::connect(&island,&IslandBridge::displayRequested,&app,[&] { ++wakes; });
    island.publishState("focus",{{"title","focus"}},20);
    island.publishState("media",{{"title","media"}},40);
    island.publishState("input",{{"title","input"}},60);
    CHECK(island.activeState().value("sourceId")=="input");
    const int before=wakes;
    for (int i=0;i<100;++i) island.publishState("input",{{"progress",i/100.0}},60);
    CHECK(wakes==before);
    island.withdraw("input");
    island.publishState("equal",{},40);
    CHECK(island.activeState().value("sourceId")=="media");
    CHECK(island.pushEvent("notice","a",{{"title","A"}},80,4000,30000));
    now=1200;
    CHECK(island.pushEvent("urgent","b",{{"title","B"}},100,1000,30000));
    CHECK(island.activeState().value("sourceId")=="urgent");
    now=2200; tick(island);
    CHECK(island.activeState().value("sourceId")=="notice");
    now=4999; tick(island);
    CHECK(island.activeState().value("mode")=="event");
    now=5000; tick(island);
    CHECK(island.activeState().value("sourceId")=="media");
    CHECK(!island.pushEvent("notice","a",{},80));
    qInfo() << "PASS priorities + stable updates + preemption preserves exact remaining time + restore + cooldown";

    now=0;
    IslandBridge queue(nullptr,[&] { return now; });
    queue.publishState("input",{},60);
    queue.pushEvent("event","a",{{"title","A"}},80,1000,10000);
    queue.pushEvent("event","b",{{"title","B"}},80,1000,10000);
    queue.pushEvent("event","b",{{"title","merged"}},80,1000,10000);
    CHECK(queue.queuedCount()==2 && queue.activeState().value("title")=="A");
    queue.dismissCurrentEvent();
    CHECK(queue.activeState().value("title")=="merged");
    CHECK(!queue.pushEvent("event","a",{},80));
    queue.dismissCurrentEvent();
    CHECK(queue.activeState().value("sourceId")=="input" && !queue.eventActive());
    queue.pushEvent("pending","expires",{},10,1000,1000);
    now=1001; tick(queue);
    CHECK(queue.queuedCount()==0);
    qInfo() << "PASS FIFO + coalesce + dismiss advances queue + dismissal cooldown + pending expiry";

    now=0;
    IslandBridge flood(nullptr,[&] { return now; });
    flood.publishState("guard",{},100);
    for (int i=0;i<16;++i) CHECK(flood.pushEvent("low",QString::number(i),{},10,1000,30000));
    CHECK(!flood.pushEvent("low","overflow",{},10));
    CHECK(flood.pushEvent("urgent","new",{},99));
    CHECK(flood.queuedCount()==16);
    flood.withdraw("guard");
    CHECK(flood.activeState().value("sourceId")=="urgent");
    CHECK(!flood.publishState("bad/id",{},1));
    flood.withdraw("urgent");
    IslandBridge presentation(nullptr,[&] { return now; });
    presentation.publishState("guard",{{"kind","awake"},{"progress",qQNaN()}},100);
    CHECK(presentation.activeState().value("kind")=="awake" && !presentation.activeState().contains("progress"));
    qInfo() << "PASS bounded queue + urgent admission + input sanitization";

    quint64 powerNow=0;
    bool allow=true;
    int expired=0, calls=0;
    AwakeManager awake(nullptr,[&](bool,bool) { ++calls; return allow; },[&] { return powerNow; });
    QObject::connect(&awake,&AwakeManager::expired,&app,[&] { ++expired; });
    awake.start(1,false);
    CHECK(awake.durationSeconds()==60);
    allow=false; awake.stop();
    CHECK(awake.active());
    allow=true; QMetaObject::invokeMethod(&awake,"tick",Qt::DirectConnection);
    CHECK(!awake.active() && expired==0);
    awake.start(1,false); powerNow=60000; allow=false;
    QMetaObject::invokeMethod(&awake,"tick",Qt::DirectConnection);
    CHECK(awake.active() && expired==0);
    allow=true; QMetaObject::invokeMethod(&awake,"tick",Qt::DirectConnection);
    CHECK(!awake.active() && expired==1);
    QMetaObject::invokeMethod(&awake,"tick",Qt::DirectConnection);
    CHECK(expired==1 && calls==6);
    qint64 focusNow=0;
    FocusTimer focus(nullptr,[&] { return focusNow; },false);
    int finished=0;
    QObject::connect(&focus,&FocusTimer::sessionCompleted,&app,[&] { ++finished; });
    focus.setDurationMinutes(1); focus.toggle(); focusNow=1500;
    CHECK(!focus.paused());
    QMetaObject::invokeMethod(&focus,"tick",Qt::DirectConnection);
    CHECK(focus.remainingSeconds()==59);
    focus.toggle(); CHECK(focus.paused()); focusNow=100000; focus.toggle();
    CHECK(focus.running() && focus.remainingSeconds()==59);
    focusNow+=59000; QMetaObject::invokeMethod(&focus,"tick",Qt::DirectConnection);
    CHECK(!focus.running() && finished==1 && focus.completedSessions()==1);
    focus.toggle(); CHECK(focus.remainingSeconds()==60);
    focus.reset(); CHECK(!focus.running() && focus.remainingSeconds()==60);
    CHECK(!focus.paused());
    focus.toggle(); focus.toggle(); CHECK(focus.paused());
    focus.reset(); CHECK(!focus.paused());
    qInfo() << "PASS mock awake: manual stop is not expiry + release retry + expiry emitted once after successful release";
    qInfo() << "ISLAND CORE PASS: no hardware observers, no power requests, no real clipboard writes";
    std::cout << "ISLAND CORE PASS: priorities, buffering, exact resume time, dismissal, urgent admission, mock expiry/retry, focus pause/resume/completion\n";
}
