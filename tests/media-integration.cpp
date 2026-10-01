#include "include/mediaworker.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <cmath>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    MediaWorker worker;
    QVariantList sessions;
    QString selected, a, b;
    bool following = true, canSeek = false;
    double position = 0, duration = 0;
    QObject::connect(&worker,&MediaWorker::sessionsUpdated,&app,[&](const QVariantList &list,const QString &id,bool follow) { sessions=list; selected=id; following=follow; });
    QObject::connect(&worker,&MediaWorker::timelineUpdated,&app,[&](double p,double d,bool seek) { position=p; duration=d; canSeek=seek; });
    QObject::connect(&worker,&MediaWorker::commandStatus,&app,[](const QString &status) { if (!status.isEmpty()) qInfo() << "MEDIA_STATUS" << status; });
    worker.initialize();
    int step = 0;
    QTimer timer; timer.setInterval(250);
    QObject::connect(&timer,&QTimer::timeout,&app,[&] {
        if (step == 0) {
            for (const auto &entry : sessions) {
                auto value=entry.toMap(); const auto name=value.value("name").toString();
                if (name.contains("MediaFixtureA")) a=value.value("id").toString();
                if (name.contains("MediaFixtureB")) b=value.value("id").toString();
            }
            if (!a.isEmpty() && !b.isEmpty()) { worker.selectSession(a); step=1; }
        } else if (step == 1 && selected == a && std::abs(duration-120)<0.1 && canSeek) { worker.seek(35,a); step=2; }
        else if (step == 2 && std::abs(position-35)<0.5) { worker.selectSession(b); step=3; }
        else if (step == 3 && selected == b && canSeek) { worker.seek(75,b); step=4; }
        else if (step == 4 && std::abs(position-75)<0.5) { worker.followSystemSession(); step=5; }
        else if (step == 5 && following) { qInfo() << "MEDIA_INTEGRATION PASS two native sessions + selection + seek + follow"; app.quit(); }
    });
    QTimer::singleShot(12000,&app,[&] { qCritical() << "MEDIA_INTEGRATION TIMEOUT step=" << step << "sessions=" << sessions << "seek=" << canSeek << "duration=" << duration << "position=" << position; app.exit(3); });
    timer.start(); return app.exec();
}
