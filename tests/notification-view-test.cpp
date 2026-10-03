#include "include/notificationviewmodel.h"
#include <QCoreApplication>
#include <QSignalSpy>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
static Notification notice(const QString &id, const QString &app, const QString &appId, int time, const QString &title, bool unread = true) {
    Notification n; n.id=id; n.appName=app; n.appId=appId; n.title=title; n.timestamp=QDateTime::fromSecsSinceEpoch(time); n.unread=unread; return n;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    NotificationModel source;
    NotificationViewModel view(&source);
    QSignalSpy removed(&source, &NotificationModel::removalRequested);
    QSignalSpy batch(&source, &NotificationModel::removalsRequested);
    QSignalSpy fullClear(&source, &NotificationModel::clearRequested);
    QSignalSpy count(&view, &NotificationViewModel::countChanged);
    auto first = notice("1", "邮件", "z.mail", 100, "预算报告", false);
    auto second = notice("2", "聊天", "a.chat", 200, "Hello [world]"); second.content="项目进度";
    auto third = notice("3", "聊天", "a.chat", 300, "第三条");
    auto sameName = notice("4", "聊天", "b.other", 150, "不同应用");
    source.synchronize({first, second, third, sameName});
    CHECK(view.count() == 4 && !view.filtered() && count.size() > 0);
    CHECK(view.index(0, 0).data(NotificationModel::IdRole).toString() == "3");
    CHECK(view.index(1, 0).data(NotificationModel::IdRole).toString() == "2");
    CHECK(view.index(0, 0).data(NotificationModel::AppGroupRole) != view.index(2, 0).data(NotificationModel::AppGroupRole));
    CHECK(view.groupTitle("id:a.chat") == "聊天");
    view.setSearchText(" hello "); CHECK(view.count() == 1 && view.filtered());
    view.removeNotificationById("2"); CHECK(removed.size() == 1 && removed.at(0).at(0).toString() == "2" && source.rowCount() == 4);
    view.setSearchText("[world]"); CHECK(view.count() == 1); // Literal, not regex.
    view.clearVisible(); CHECK(batch.size() == 1 && batch.at(0).at(0).toStringList() == QStringList{"2"});
    CHECK(fullClear.size() == 0 && source.rowCount() == 4);
    view.setSearchText("项目进度"); CHECK(view.count() == 1);
    view.setSearchText("邮件"); CHECK(view.count() == 1);
    view.setUnreadOnly(true); CHECK(view.count() == 0);
    view.setSearchText(""); CHECK(view.count() == 3);
    view.markReadById("2", true); CHECK(view.count() == 2 && source.unreadCount() == 2);
    view.markVisibleAsRead(); CHECK(view.count() == 0 && source.unreadCount() == 0);
    view.markReadById("1", false); CHECK(view.count() == 1);
    source.removeById("1");
    view.markReadById("1", true); view.removeNotificationById("1"); CHECK(removed.size() == 1 && source.rowCount() == 3);
    view.setUnreadOnly(false); view.setSearchText("不存在"); view.clearVisible(); CHECK(batch.size() == 1);
    view.setSearchText("");
    auto newItem = notice("5", "新应用", "c.new", 400, "更新"); source.addNotification(newItem); CHECK(view.count() == 4);
    view.setSearchText("更新"); view.clearVisible(); CHECK(batch.size() == 2 && batch.at(1).at(0).toStringList() == QStringList{"5"});
    newItem.title="新标题"; source.addNotification(newItem); CHECK(view.count() == 0);
    view.setSearchText("新标题"); CHECK(view.count() == 1);
    auto unknown = notice("6", "", "", 500, "匿名"); source.addNotification(unknown);
    view.setSearchText(""); CHECK(view.groupTitle("name:") == "未提供应用名称");
    source.synchronize({}); CHECK(view.count() == 0);
    auto locked = notice("7", "应用", "app.locked", 600, "不可删除"); locked.removable=false;
    auto removable = notice("8", "应用", "app.locked", 700, "可删除");
    source.synchronize({locked, removable});
    source.requestRemovalByIds({"7", "8", "8", "missing", ""});
    CHECK(batch.size() == 3 && batch.at(2).at(0).toStringList() == QStringList{"8"});
    CHECK(source.rowCount() == 2 && fullClear.size() == 0);
    std::cout << "PASS notification view: stable app identity, newest-in-group, literal search, unread live updates, ID-safe actions, filtered batch scope, no global clear, stale ID, inserts/updates/reset; no system APIs\n";
    return 0;
}
