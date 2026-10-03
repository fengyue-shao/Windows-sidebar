#include "include/notificationmodel.h"
#include <QCoreApplication>
#include <QSignalSpy>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
static Notification notice(const QString &id) { Notification n; n.id = id; n.title = id; return n; }
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    NotificationModel model;
    QSignalSpy remove(&model, &NotificationModel::removalRequested);
    QSignalSpy clear(&model, &NotificationModel::clearRequested);
    model.addNotification(notice("1")); model.addNotification(notice("2"));
    model.removeNotification(0);
    CHECK(remove.size() == 1 && remove.at(0).at(0).toString() == "1");
    CHECK(model.rowCount() == 2 && model.unreadCount() == 2); // Request/failure retains rows.
    model.clear(); CHECK(clear.size() == 1 && model.rowCount() == 2);
    model.removeNotification(-1); model.removeNotification(8); CHECK(remove.size() == 1);
    model.markAsRead(1);
    model.synchronize({notice("2"), notice("3"), notice("3"), notice("")});
    CHECK(model.rowCount() == 2 && !model.contains("1") && model.contains("3"));
    CHECK(!model.notification(0).unread && model.unreadCount() == 1);
    CHECK(remove.size() == 1 && clear.size() == 1); // Sync cannot recursively request deletion.
    model.removeById("3"); CHECK(model.rowCount() == 1 && model.unreadCount() == 0);
    model.synchronize({}); CHECK(model.rowCount() == 0 && model.unreadCount() == 0);
    model.clear(); CHECK(clear.size() == 1);
    auto fixed = notice("4"); fixed.removable = false;
    model.addNotification(fixed); model.removeNotification(0); CHECK(remove.size() == 1);
    std::cout << "PASS notification model: requests retain rows, confirmation sync, unread preservation, stale removal, deduplication, invalid rows, no request recursion; no system APIs\n";
    return 0;
}
