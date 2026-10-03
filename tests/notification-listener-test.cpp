#include "include/notificationlistener.h"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
// Only manufactured async results. Never call start(), Current(), or a real listener API.
class NotificationListenerTest {
    static NotificationListener::AccessOperation access(bool allowed, bool fail = false) {
        if (fail) throw winrt::hresult_access_denied();
        co_return allowed ? winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus::Allowed
                          : winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus::Denied;
    }
    static NotificationListener::ReadOperation emptyRead() {
        co_return winrt::single_threaded_vector<winrt::Windows::UI::Notifications::UserNotification>().GetView();
    }
public:
    static int run() {
        NotificationListener listener;
        QSignalSpy denied(&listener, &NotificationListener::accessDenied);
        QSignalSpy errors(&listener, &NotificationListener::errorOccurred);
        QSignalSpy snapshots(&listener, &NotificationListener::snapshotReady);
        auto setup = [&] {
            listener.m_session = std::make_shared<std::atomic_bool>(true);
            listener.m_starting = true;
            return listener.m_session;
        };
        auto session = setup();
        NotificationListener::awaitPermission(&listener, session, access(false));
        QTest::qWait(30);
        CHECK(denied.size() == 1 && listener.failed() && !listener.busy());
        session = setup();
        NotificationListener::awaitPermission(&listener, session, access(false, true));
        QTest::qWait(30);
        CHECK(errors.size() == 1 && listener.failed() && !listener.busy());
        session = setup();
        NotificationListener::awaitPermission(&listener, session, access(true));
        listener.stop(); // Allowed result is queued but must not activate a real listener.
        QTest::qWait(30);
        CHECK(!session->load() && !listener.active() && !listener.busy() && !listener.failed());
        CHECK(denied.size() == 1 && errors.size() == 1);
        session = setup();
        NotificationListener::awaitPermission(&listener, session, access(false));
        auto replacement = setup(); // Rapid off/on session replacement rejects the old reply.
        QTest::qWait(30);
        CHECK(denied.size() == 1 && listener.current(replacement) && !listener.current(session));
        listener.stop();
        session = setup();
        NotificationListener::awaitSnapshot(&listener, session, emptyRead(), 0);
        listener.stop();
        QTest::qWait(30);
        CHECK(snapshots.size() == 0 && !listener.active() && !listener.busy());
        listener.removeNotification("1");
        CHECK(listener.failed());
        listener.stop(); listener.clearNotifications(); CHECK(listener.failed());
        listener.stop();
        std::cout << "PASS notification lifecycle: denied/error completion, stop before permission/read delivery, replaced session, unavailable deletion; manufactured async results only\n";
        return 0;
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    return NotificationListenerTest::run();
}
