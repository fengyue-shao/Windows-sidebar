#ifndef NOTIFICATIONLISTENER_H
#define NOTIFICATIONLISTENER_H

#include <QObject>
#include <QString>
#include "Notification.h"
#include <winrt/Windows.UI.Notifications.Management.h>

class NotificationListener : public QObject
{
    Q_OBJECT
public:
    explicit NotificationListener(QObject *parent = nullptr);
    ~NotificationListener() override;

    void start();

public slots:
    void removeNotification(const QString &id);
    void clearNotifications();

signals:
    void notificationAdded(const Notification &notification);
    void notificationRemoved(const QString &id);
    void notificationUpdated(const Notification &notification);
    void accessDenied();
    void errorOccurred(const QString &message);

private:
    using Listener = winrt::Windows::UI::Notifications::Management::UserNotificationListener;

    winrt::Windows::Foundation::IAsyncAction requestPermissionAndStart();
    winrt::Windows::Foundation::IAsyncAction loadNotifications();
    Notification toNotification(
        const winrt::Windows::UI::Notifications::UserNotification &notification) const;
    void reportException(const winrt::hresult_error &error);

    Listener m_listener{nullptr};
    winrt::event_token m_notificationChangedToken{};
    bool m_started = false;
};

#endif // NOTIFICATIONLISTENER_H
