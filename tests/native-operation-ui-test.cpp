#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QtTest/QTest>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
class OperationFixture : public QQmlPropertyMap {
    Q_OBJECT
public:
    OperationFixture() : QQmlPropertyMap(this, nullptr) {}
    int cancels = 0, clears = 0, refreshes = 0;
    Q_INVOKABLE void cancel() { ++cancels; insert("busy", false); insert("status", "已停止等待，不保证 Windows 操作已撤回"); }
    Q_INVOKABLE void clear() { ++clears; insert("text", ""); insert("status", ""); }
    Q_INVOKABLE void refresh() { ++refreshes; }
};
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv); if (argc != 4) return 2;
    QQmlPropertyMap theme; OperationFixture ocr, hotspot, clipboard;
    const QVariantMap colors{{"accent", "#8CCBFF"}, {"accentSoft", "#203D59"}, {"border", "#304052"},
        {"cardSurface", "#17202B"}, {"surface", "#182330"}, {"surfaceStrong", "#1D2A38"},
        {"textPrimary", "#FFFFFF"}, {"textSecondary", "#DDE8F4"}, {"textMuted", "#B5C1D0"},
        {"textFaint", "#99AABB"}, {"warning", "#F5CB80"}, {"dangerSoft", "#44262E"}, {"danger", "#FF939A"}, {"overlayStart", "#101720"}};
    for (auto it=colors.cbegin(); it!=colors.cend(); ++it) theme.insert(it.key(), it.value());
    clipboard.insert("items", QVariantList{}); clipboard.insert("loading", false); clipboard.insert("statusText", "模拟历史，无系统读取");
    ocr.insert("busy", true); ocr.insert("text", "");
    ocr.insert("status", "正在本地识别原图。此处是较长状态说明，用于核验窄窗口下停止按钮与文字不会裁切。识别最多等待三十秒，旧结果不会覆盖新请求。");
    hotspot.insert("busy", true); hotspot.insert("supported", true); hotspot.insert("active", false); hotspot.insert("ssid", "模拟热点");
    hotspot.insert("status", "Windows 正在处理热点操作；停止等待不保证系统操作已撤回，请核验实际状态，不自动重试。");
    QQuickView view;
    view.rootContext()->setContextProperty("themeManager", &theme);
    view.rootContext()->setContextProperty("clipboardOcr", &ocr);
    view.rootContext()->setContextProperty("clipboardManager", &clipboard);
    view.rootContext()->setContextProperty("hotspotManager", &hotspot);
    view.rootContext()->setContextProperty("uiSelfTest", false);
    int warnings = 0;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &errors) {
        warnings += errors.size(); for (const auto &error : errors) std::cerr << error.toString().toStdString() << '\n';
    });
    view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(360, 640); view.setColor(QColor("#101720"));
    QImage sheet(384, 4*660, QImage::Format_ARGB32_Premultiplied); sheet.fill(QColor("#101720"));
    auto capture = [&](int row) { QPainter painter(&sheet); painter.drawImage(QPoint(12, row*660+10), view.grabWindow()); };
    auto click = [&](QQuickItem *item) { QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint()); QTest::qWait(100); };
    view.setSource(QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    CHECK(view.rootObject()); view.show(); QTest::qWait(250);
    auto *close = view.rootObject()->findChild<QQuickItem *>("ocrCancelOrClose");
    auto *status = view.rootObject()->findChild<QQuickItem *>("ocrStatus");
    CHECK(close && status && close->isEnabled() && close->property("text").toString() == "停止识别");
    auto *card = status->parentItem()->parentItem()->parentItem();
    CHECK(status->mapToItem(card, QPointF(0, status->height())).y() <= card->height());
    CHECK(close->mapToScene(QPointF(close->width(), close->height())).x() <= view.width());
    capture(0); click(close); CHECK(ocr.cancels == 1 && close->property("text").toString() == "关闭"); capture(1);
    click(close); CHECK(ocr.clears == 1 && ocr.value("status").toString().isEmpty());
    ocr.insert("text", "模拟识别结果\n可编辑和复制"); ocr.insert("status", "本地识别完成"); QTest::qWait(100); capture(2);
    view.setSource(QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath()));
    CHECK(view.rootObject()); QTest::qWait(250);
    auto *cancel = view.rootObject()->findChild<QQuickItem *>("hotspotCancel");
    auto *toggle = view.rootObject()->findChild<QQuickItem *>("hotspotToggle");
    auto *refresh = view.rootObject()->findChild<QQuickItem *>("hotspotRefresh");
    CHECK(cancel && toggle && refresh && cancel->isVisible() && !toggle->isEnabled() && !refresh->isEnabled());
    CHECK(cancel->mapToScene(QPointF(cancel->width(), cancel->height())).x() <= view.width());
    capture(3); click(cancel); CHECK(hotspot.cancels == 1 && !cancel->isVisible() && toggle->isEnabled() && refresh->isEnabled());
    CHECK(warnings == 0);
    const auto output = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();
    CHECK(QDir().mkpath(QFileInfo(output).absolutePath()) && sheet.save(output));
    std::cout << "PASS production OCR/hotspot cards: stop/close routing, busy controls, narrow long status, result editor, no QML warnings; fixtures only\n";
    return 0;
}
#include "native-operation-ui-test.moc"
