#include "include/audiooutputswitch.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QtTest/QTest>
#include <iostream>

#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
static void fill(QQmlPropertyMap &map, const QVariantMap &values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it) map.insert(it.key(), it.value());
}
static QQuickItem *findVisual(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = findVisual(child, name)) return found;
    return nullptr;
}
class WifiFixture : public QQmlPropertyMap {
    Q_OBJECT
public:
    WifiFixture() : QQmlPropertyMap(this, nullptr) {}
    int hiddenCalls = 0, forgetCalls = 0;
    bool confirmed = false;
    QByteArray lastAdapter;
    Q_INVOKABLE void connectHiddenNetwork(const QString &, const QString &, bool, const QByteArray &adapter) { ++hiddenCalls; lastAdapter=adapter; }
    Q_INVOKABLE void forgetNetwork(const QString &, const QString &, const QByteArray &adapter, bool confirmation) { ++forgetCalls; lastAdapter=adapter; confirmed=confirmation; }
};
class BluetoothFixture : public QQmlPropertyMap {
    Q_OBJECT
public:
    BluetoothFixture() : QQmlPropertyMap(this, nullptr) {}
    int scans = 0, scanStops = 0, operationStops = 0, pairs = 0, unpairs = 0;
    QString target;
    Q_INVOKABLE void scan() { ++scans; insert("scanning", true); }
    Q_INVOKABLE void cancelScan() { ++scanStops; insert("scanning", false); }
    Q_INVOKABLE void cancelOperation() { ++operationStops; insert("busy", false); }
    Q_INVOKABLE void pairDevice(const QString &id) { ++pairs; target = id; insert("busy", true); }
    Q_INVOKABLE void unpairDevice(const QString &id) { ++unpairs; target = id; insert("busy", true); }
};
int main(int argc, char **argv)
{
    // Only QML property fixtures; no production managers or system calls.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    if (argc != 3) return 2;
    QQmlPropertyMap theme, tray, audio, settings, radio, network, dropdown;
    BluetoothFixture bluetooth;
    WifiFixture wifi;
    fill(theme, {{"accent", "#8CCBFF"}, {"accentSoft", "#203D59"}, {"border", "#304052"},
        {"borderStrong", "#526174"}, {"cardSurface", "#17202B"}, {"danger", "#FF939A"},
        {"dangerSoft", "#44262E"}, {"overlayStart", "#101720"}, {"success", "#80DAB2"},
        {"surfaceHover", "#253548"}, {"surfaceStrong", "#1D2A38"}, {"textFaint", "#99AABB"},
        {"textMuted", "#B5C1D0"}, {"textPrimary", "#FFFFFF"}, {"textSecondary", "#DDE8F4"},
        {"track", "#304052"}, {"warning", "#F5CB80"}, {"mode", 2}});
    fill(tray, {{"volume", 45}, {"muted", false}, {"audioDeviceName", "模拟扬声器"},
        {"online", true}, {"batteryPresent", false}, {"batteryPercent", 80}, {"charging", false},
        {"audioSessions", QVariantList{}}, {"audioOutputs", QVariantList{QVariantMap{
            {"id", "fixture-output"}, {"name", "模拟扬声器"}, {"selected", true}}}},
        {"audioOutputStatus", ""}, {"audioOutputSwitchFailed", false}});
    fill(audio, {{"outputPeak", 0.0}, {"outputMeterAvailable", false}});
    fill(settings, {{"panelOpacity", 1.0}, {"compactMode", false}, {"panelWidth", 420}, {"autoHide", false}, {"suppressEdgeInFullscreen", false}, {"activationShortcut", "Ctrl+Alt+Space"}});
    fill(dropdown, {{"hotkeyStatus", "模拟快捷键状态"}, {"shortcutText", "Ctrl+Alt+Space"}});
    fill(wifi, {{"networks", QVariantList{}}, {"adapters", QVariantList{}}, {"scanning", false}, {"connecting", false}, {"statusText", "模拟网络"}});
    fill(bluetooth, {{"devices", QVariantList{}}, {"scanning", false}, {"busy", false}, {"statusText", "模拟蓝牙"}});
    fill(radio, {{"busy", false}, {"bluetoothEnabled", false}, {"wifiEnabled", true}, {"statusText", ""}});
    fill(network, {{"downloadSpeedText", "0 KB/s"}, {"uploadSpeedText", "0 KB/s"}});
    QQuickView view;
    for (const auto &entry : QList<QPair<QString, QObject *>>{{"themeManager", &theme}, {"systemTrayManager", &tray},
        {"audioInputManager", &audio}, {"uiSettings", &settings}, {"wifiManager", &wifi},
        {"bluetoothManager", &bluetooth}, {"radioManager", &radio}, {"networkMonitor", &network}, {"dropdownController", &dropdown}})
        view.rootContext()->setContextProperty(entry.first, entry.second);
    int warnings = 0;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &errors) {
        warnings += errors.size();
        for (const auto &error : errors) std::cerr << error.toString().toStdString() << '\n';
    });
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(420, 640); view.setColor(QColor("#101720"));
    view.setSource(QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    CHECK(view.rootObject() && view.status() != QQuickView::Error);
    view.rootObject()->setProperty("expanded", true);
    view.show(); QTest::qWait(350);
    auto *status = view.rootObject()->findChild<QQuickItem *>("audioOutputSwitchStatus");
    CHECK(status && !status->isVisible());
    const auto failed = SidebarAudio::switchOutput(QStringLiteral("fixture-output"),
        [](ERole) { return E_ACCESSDENIED; },
        [](ERole) { return SidebarAudio::EndpointRead{E_FAIL, {}}; });
    QImage sheet(764, 8 * 660, QImage::Format_ARGB32_Premultiplied);
    sheet.fill(QColor("#101720"));
    int row = 0;
    for (const auto &entry : QList<QPair<QString, bool>>{{QStringLiteral("输出设备切换已确认"), false},
        {QStringLiteral("输出设备已不可用，或设备列表读取失败；请刷新或打开 Windows 声音设置"), true},
        {failed.message, true}}) {
        tray.insert("audioOutputStatus", entry.first);
        tray.insert("audioOutputSwitchFailed", entry.second);
        view.resize(row == 2 ? 360 : 740, 640);
        QTest::qWait(350);
        CHECK(status->isVisible() && status->property("text").toString() == entry.first);
        const auto position = status->mapToScene(QPointF(0, 0));
        CHECK(position.x() >= 0 && position.y() >= 0 && status->width() > 0);
        CHECK(position.x() + status->width() <= view.width() + 1);
        CHECK(position.y() + status->height() <= view.height());
        CHECK(status->property("color").value<QColor>() == QColor(entry.second ? "#F5CB80" : "#B5C1D0"));
        if (row == 2) CHECK(status->property("lineCount").toInt() > 1);
        const auto frame = view.grabWindow(); CHECK(!frame.isNull());
        QPainter painter(&sheet); painter.drawImage(QPoint(12, row++ * 660 + 10), frame);
    }
    tray.insert("audioOutputStatus", ""); QTest::qWait(50);
    CHECK(!status->isVisible() && warnings == 0);
    const QByteArray adapter(16, 'a');
    wifi.insert("adapters", QVariantList{QVariantMap{{"name", "模拟适配器"}, {"interfaceId", adapter}}});
    view.rootObject()->setProperty("currentPanel", 1);
    view.resize(360, 640);
    wifi.insert("statusText", "Wi-Fi 连接失败（原因 12345）：模拟长错误说明，Windows 尚未确认连接；请刷新或在系统设置检查");
    view.rootObject()->setProperty("selectedWifiSsid", "Mock Wi-Fi");
    view.rootObject()->setProperty("selectedWifiProfile", "mock-profile");
    view.rootObject()->setProperty("selectedWifiAdapter", adapter);
    view.rootObject()->setProperty("selectedWifiConnected", true);
    QTest::qWait(350);
    auto click = [&](const char *name) {
        auto *item = findVisual(view.rootObject(), QString::fromLatin1(name));
        if (!item || !item->isVisible() || !item->isEnabled()) { std::cerr << name << " unavailable visible=" << (item && item->isVisible()) << " enabled=" << (item && item->isEnabled()) << '\n'; return false; }
        const auto pos = item->mapToScene(QPointF(item->width()/2, item->height()/2));
        std::cerr << name << " center=" << pos.x() << "," << pos.y() << " size=" << item->width() << "," << item->height() << '\n';
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        QTest::qWait(100); return true;
    };
    CHECK(click("wifiForgetButton") && view.rootObject()->property("confirmWifiForget").toBool());
    CHECK(wifi.forgetCalls == 0);
    auto frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 3*660+10), frame); }
    CHECK(click("wifiForgetConfirm") && wifi.forgetCalls == 1 && wifi.confirmed && wifi.lastAdapter == adapter);
    view.rootObject()->setProperty("showHiddenWifi", true); QTest::qWait(150);
    auto *ssid = view.rootObject()->findChild<QQuickItem *>("hiddenWifiSsid");
    auto *password = view.rootObject()->findChild<QQuickItem *>("hiddenWifiPassword");
    auto *hidden = view.rootObject()->findChild<QQuickItem *>("hiddenWifiConnect");
    CHECK(ssid && password && hidden && !hidden->isEnabled());
    ssid->setProperty("text", "Hidden test"); password->setProperty("text", "mock-pass"); QTest::qWait(100);
    CHECK(hidden->isEnabled());
    wifi.insert("connecting", true); QTest::qWait(50); CHECK(!hidden->isEnabled());
    wifi.insert("connecting", false); QTest::qWait(50);
    CHECK(click("hiddenWifiConnect") && wifi.hiddenCalls == 1 && wifi.lastAdapter == adapter);
    frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 4*660+10), frame); }
    CHECK(warnings == 0);
    view.rootObject()->setProperty("currentPanel", 3); QTest::qWait(350);
    auto *shortcutField = view.rootObject()->findChild<QQuickItem *>("activationShortcutField");
    auto *shortcutStatus = view.rootObject()->findChild<QQuickItem *>("activationShortcutStatus");
    CHECK(shortcutField && shortcutStatus && shortcutField->isVisible() && shortcutStatus->isVisible());
    CHECK(shortcutStatus->mapToScene(QPointF(0, shortcutStatus->height())).y() < view.height());
    CHECK(shortcutField->property("text").toString() == "Ctrl+Alt+Space");
    frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 5*660+10), frame); }
    view.rootObject()->setProperty("currentPanel", 4);
    bluetooth.insert("devices", QVariantList{
        QVariantMap{{"id", "paired"}, {"name", "模拟已配对设备"}, {"paired", true}, {"canPair", false}, {"connected", true}, {"signal", -30}},
        QVariantMap{{"id", "candidate"}, {"name", "可配对设备名称较长时仍保留右侧按钮"}, {"paired", false}, {"canPair", true}, {"connected", false}, {"signal", -60}},
        QVariantMap{{"id", "unavailable"}, {"name", "尚不可配对"}, {"paired", false}, {"canPair", false}, {"connected", false}, {"signal", -80}}});
    bluetooth.insert("statusText", "初始枚举完成 · 3 个设备 · 继续发现至扫描窗口结束");
    QTest::qWait(350);
    auto *refresh = view.rootObject()->findChild<QQuickItem *>("bluetoothRefreshButton");
    auto *stop = view.rootObject()->findChild<QQuickItem *>("bluetoothStopButton");
    auto *candidate = findVisual(view.rootObject(), "bluetoothAction_candidate");
    auto *paired = findVisual(view.rootObject(), "bluetoothAction_paired");
    auto *unavailable = findVisual(view.rootObject(), "bluetoothAction_unavailable");
    CHECK(refresh && stop && candidate && paired && unavailable && !unavailable->isEnabled());
    CHECK(click("bluetoothRefreshButton") && bluetooth.scans == 1 && !refresh->isEnabled() && stop->isVisible());
    frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 6*660+10), frame); }
    CHECK(click("bluetoothStopButton") && bluetooth.scanStops == 1 && !stop->isVisible());
    CHECK(click("bluetoothAction_candidate") && bluetooth.pairs == 1 && bluetooth.target == "candidate");
    CHECK(!candidate->isEnabled() && !paired->isEnabled() && !refresh->isEnabled() && stop->isVisible());
    bluetooth.insert("statusText", "正在配对，请留意 Windows 或设备确认提示；停止等待不保证系统操作已撤回");
    QTest::qWait(100);
    frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 7*660+10), frame); }
    CHECK(click("bluetoothStopButton") && bluetooth.operationStops == 1 && candidate->isEnabled());
    CHECK(click("bluetoothAction_paired") && bluetooth.unpairs == 1 && bluetooth.target == "paired");
    CHECK(warnings == 0);
    const QString output = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    CHECK(QDir().mkpath(QFileInfo(output).absolutePath()) && sheet.save(output));
    std::cout << "PASS production sound/Wi-Fi/layout/Bluetooth cards: scoped actions, scan/stop, pair/unpair ID forwarding, busy gates, long status, narrow layout, zero QML warnings; mock-only offscreen\n";
    return 0;
}
#include "audio-output-ui-test.moc"
