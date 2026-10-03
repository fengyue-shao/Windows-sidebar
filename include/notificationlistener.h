#ifndef NOTIFICATIONLISTENER_H
#define NOTIFICATIONLISTENER_H

#include <QObject>
#include <QString>
#include <QPointer>
#include <QVector>
#include <QSet>
#include <atomic>
#include <memory>
#include "Notification.h"
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.Foundation.Collections.h>

class NotificationListener : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool active READ active NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY statusChanged)
public:
    explicit NotificationListener(QObject *parent = nullptr);
    ~NotificationListener() override;

    QString statusText() const { return m_statusText; }
    bool active() const { return m_started; }
    bool busy() const { return m_starting || m_reading; }
    bool failed() const { return m_failed; }
    Q_INVOKABLE void start();
    void stop();
    Q_INVOKABLE void synchronize();

public slots:
    void removeNotification(const QString &id);
    void removeNotifications(const QStringList &ids);
    void clearNotifications();

signals:
    void notificationAdded(const Notification &notification);
    void notificationArrived(const Notification &notification);
    void notificationRemoved(const QString &id);
    void notificationUpdated(const Notification &notification);
    void snapshotReady(const QVector<Notification> &notifications);
    void accessDenied();
    void errorOccurred(const QString &message);
    void statusChanged();

private:
    friend class NotificationListenerTest;
    using Listener = winrt::Windows::UI::Notifications::Management::UserNotificationListener;

    using AccessOperation = winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus>;
    using ReadOperation = winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::UI::Notifications::UserNotification>>;
    using Session = std::shared_ptr<std::atomic_bool>;
    static winrt::fire_and_forget awaitPermission(QPointer<NotificationListener> self, Session session, AccessOperation operation);
    static winrt::fire_and_forget awaitSnapshot(QPointer<NotificationListener> self, Session session, ReadOperation operation, quint64 revision);
    static Notification toNotification(const winrt::Windows::UI::Notifications::UserNotification &notification);
    void activate(Session session);
    void changed(Session session, Listener listener, uint id, bool removed);
    void setStatus(const QString &message, bool failed = false);
    void reportException(const winrt::hresult_error &error);
    bool current(const Session &session) const { return session == m_session && session && session->load(); }

    Listener m_listener{nullptr};
    winrt::event_token m_notificationChangedToken{};
    AccessOperation m_permission{nullptr};
    ReadOperation m_read{nullptr};
    Session m_session;
    quint64 m_revision = 0;
    bool m_started = false, m_starting = false, m_reading = false, m_failed = false;
    bool m_clearPending = false;
    QSet<QString> m_pendingRemoval;
    QStringList m_deleteErrors;
    QString m_statusText = QStringLiteral("通知监听尚未开启");
};

#endif // NOTIFICATIONLISTENER_H
