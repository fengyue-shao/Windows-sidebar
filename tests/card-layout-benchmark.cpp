#include "include/uicardmanager.h"
#include <QtTest/QTest>
#include <QTemporaryDir>
#include <memory>

class CardLayoutBenchmark : public QObject {
    Q_OBJECT
    void rows() {
        QTest::addColumn<int>("count");
        QTest::newRow("9-cards")<<9;
        QTest::newRow("32-cards")<<32;
        QTest::newRow("64-cards")<<64;
    }
    std::unique_ptr<UiCardManager> create(int count,const QString &path) {
        auto manager=std::make_unique<UiCardManager>(nullptr,path);
        for(int i=0;i<count;++i) {
            QVariantMap definition{{"id",QString("card.%1").arg(i)},{"page","monitor"},{"title","Benchmark"},
                {"minimumWidth",200},{"minimumHeight",96},{"defaultHeight",144},{"maximumHeight",640}};
            if(!manager->registerCard(definition)) return {};
        }
        return manager;
    }
private slots:
    void geometryBatch_data() { rows(); }
    void geometryBatch() {
        QFETCH(int,count); QTemporaryDir directory; auto manager=create(count,directory.filePath("layout.json")); QVERIFY(manager);
        qreal sum=0;
        QBENCHMARK { sum=manager->contentHeight("monitor",680); for(int i=0;i<count;++i) sum+=manager->geometry(QString("card.%1").arg(i),680).value("height").toDouble(); }
        QVERIFY(sum>0);
    }
    void moveAndResolve_data() { rows(); }
    void moveAndResolve() {
        QFETCH(int,count); QTemporaryDir directory; auto manager=create(count,directory.filePath("layout.json")); QVERIFY(manager);
        QVERIFY(manager->beginEdit("monitor",680)); int iteration=0; bool accepted=false; qreal sum=0;
        QBENCHMARK {
            accepted=manager->setGeometry("card.0",0,(++iteration%2)*8,320,144,680);
            sum=manager->contentHeight("monitor",680);
            for(int i=0;i<count;++i) sum+=manager->geometry(QString("card.%1").arg(i),680).value("height").toDouble();
        }
        QVERIFY(accepted); QVERIFY(sum>0);
    }
};
QTEST_GUILESS_MAIN(CardLayoutBenchmark)
#include "card-layout-benchmark.moc"
