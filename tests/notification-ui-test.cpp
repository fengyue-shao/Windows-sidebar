#include "include/notificationmodel.h"
#include "include/notificationviewmodel.h"
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
#include <QSignalSpy>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv); if (argc != 3) return 2;
    QQmlPropertyMap theme, service;
    const QVariantMap colors{{"accent", "#8CCBFF"}, {"border", "#304052"}, {"borderStrong", "#526174"},
        {"cardSurface", "#17202B"}, {"danger", "#FF939A"}, {"dangerSoft", "#44262E"},
        {"surfaceHover", "#253548"}, {"surfaceStrong", "#1D2A38"}, {"textMuted", "#B5C1D0"},
        {"textPrimary", "#FFFFFF"}, {"textSecondary", "#DDE8F4"}, {"warning", "#F5CB80"},
        {"track", "#304052"}, {"textFaint", "#99AABB"}, {"overlayStart", "#101720"}};
    for (auto it=colors.cbegin(); it!=colors.cend(); ++it) theme.insert(it.key(), it.value());
    service.insert("statusText", "正在同步 Windows 通知…"); service.insert("active", true);
    service.insert("busy", true); service.insert("failed", false);
    NotificationModel model;
    NotificationViewModel filtered(&model);
    QSignalSpy batch(&model, &NotificationModel::removalsRequested);
    QQuickView view; view.rootContext()->setContextProperty("themeManager", &theme);
    int warnings = 0;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &errors) {
        warnings += errors.size(); for (const auto &error : errors) std::cerr << error.toString().toStdString() << '\n';
    });
    view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(360, 640);
    view.setColor(QColor("#101720")); view.setSource(QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    CHECK(view.rootObject() && view.status() != QQuickView::Error);
    view.rootObject()->setProperty("model", QVariant::fromValue<QObject *>(&model));
    view.rootObject()->setProperty("viewModel", QVariant::fromValue<QObject *>(&filtered));
    view.rootObject()->setProperty("service", QVariant::fromValue<QObject *>(&service));
    view.show(); QTest::qWait(100);
    auto *status = view.rootObject()->findChild<QQuickItem *>("notificationServiceStatus"); CHECK(status);
    QImage sheet(764, 3960, QImage::Format_ARGB32_Premultiplied); sheet.fill(QColor("#101720"));
    const QStringList texts{"正在同步 Windows 通知…", "未获得通知访问权限；可在 Windows 设置中允许访问后重试", "通知同步失败，保留现有列表：模拟接口拒绝访问，已有通知不会被当成空列表清除"};
    for (int row=0; row<3; ++row) {
        service.insert("statusText", texts[row]); service.insert("failed", row > 0);
        service.insert("busy", row == 0); service.insert("active", row != 1);
        if (row == 2) { Notification n; n.id="1"; n.appName="模拟应用"; n.title="保留的通知"; n.content="失败后仍然显示"; model.addNotification(n); }
        view.resize(row == 0 ? 740 : 360, 640); QTest::qWait(150);
        CHECK(status->isVisible() && status->property("text").toString() == texts[row]);
        const auto pos = status->mapToScene(QPointF(0, 0));
        CHECK(pos.x() >= 0 && pos.y() >= 0 && pos.x()+status->width() <= view.width()+1);
        CHECK(pos.y()+status->height() < view.height());
        CHECK(status->property("color").value<QColor>() == QColor(row > 0 ? "#F5CB80" : "#B5C1D0"));
        const auto frame = view.grabWindow(); CHECK(!frame.isNull());
        QPainter painter(&sheet); painter.drawImage(QPoint(12, row*660+10), frame);
    }
    auto makeNotice = [](const QString &id, const QString &name, const QString &title, int time, bool unread = true) {
        Notification n; n.id=id; n.appId=name; n.appName=name; n.title=title;
        n.timestamp=QDateTime::fromSecsSinceEpoch(time); n.unread=unread; return n;
    };
    model.synchronize({makeNotice("10", "邮件", "Report budget", 100), makeNotice("11", "聊天", "你好", 200), makeNotice("12", "邮件", "Earlier report", 90, false)});
    service.insert("busy", false); service.insert("active", true); service.insert("failed", false); service.insert("statusText", "已同步 Windows 通知");
    auto *search = view.rootObject()->findChild<QQuickItem *>("notificationSearchField"); CHECK(search);
    auto click = [&](const char *name) {
        auto *item = view.rootObject()->findChild<QQuickItem *>(QString::fromLatin1(name));
        if (!item) return false;
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        QTest::qWait(100); return true;
    };
    QTest::qWait(150);
    CHECK(filtered.count() == 3);
    auto frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 3*660+10), frame); }
    search->forceActiveFocus();
    for (const auto key : {Qt::Key_R, Qt::Key_E, Qt::Key_P, Qt::Key_O, Qt::Key_R, Qt::Key_T}) QTest::keyClick(&view, key);
    QTest::qWait(150);
    CHECK(filtered.searchText() == "report" && filtered.count() == 2);
    CHECK(click("notificationUnreadFilter")); CHECK(filtered.unreadOnly() && filtered.count() == 1);
    CHECK(click("notificationClearVisible"));
    CHECK(batch.size() == 1 && batch.at(0).at(0).toStringList() == QStringList{"10"} && model.rowCount() == 3);
    frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 4*660+10), frame); }
    CHECK(click("notificationMarkVisibleRead")); CHECK(filtered.count() == 0 && model.unreadCount() == 1);
    CHECK(click("notificationResetFilters")); CHECK(!filtered.filtered() && filtered.count() == 3 && search->property("text").toString().isEmpty());
    filtered.setSearchText("没有这样的通知"); QTest::qWait(150); CHECK(filtered.count() == 0);
    frame = view.grabWindow(); CHECK(!frame.isNull());
    { QPainter painter(&sheet); painter.drawImage(QPoint(12, 5*660+10), frame); }
    CHECK(warnings == 0);
    const auto path = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    CHECK(QDir().mkpath(QFileInfo(path).absolutePath()) && sheet.save(path));
    std::cout << "PASS production notification card: status, grouping, keyboard search, unread/reset clicks, filtered batch scope, no-match state, retained notification, narrow layout, colors, zero warnings; mock-only offscreen\n";
    return 0;
}
