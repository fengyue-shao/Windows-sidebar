#include "include/notificationlistener.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <chrono>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::UI::Notifications::Management;

NotificationListener::NotificationListener(QObject *parent)
    : QObject{parent}
{}

NotificationListener::~NotificationListener()
{
    if (m_listener && m_started) {
        m_listener.NotificationChanged(m_notificationChangedToken);
    }
}

void NotificationListener::start()
{
    if (m_started) {
        return;
    }

    m_listener = Listener::Current();
    requestPermissionAndStart(m_listener);
}

void NotificationListener::removeNotification(const QString &id)
{
    if (!m_started)
        return;

    bool ok = false;
    const uint notificationId = id.toUInt(&ok);
    if (!ok)
        return;

    try {
        m_listener.RemoveNotification(notificationId);
    } catch (const winrt::hresult_error &error) {
        reportException(error);
    }
}

void NotificationListener::clearNotifications()
{
    if (!m_started)
        return;

    try {
        m_listener.ClearNotifications();
    } catch (const winrt::hresult_error &error) {
        reportException(error);
    }
}

winrt::fire_and_forget NotificationListener::requestPermissionAndStart(Listener listener)
{
    QPointer<NotificationListener> self(this);

    try {
        const auto access = co_await listener.RequestAccessAsync();
        if (!self) {
            co_return;
        }

        if (access != UserNotificationListenerAccessStatus::Allowed) {
            QMetaObject::invokeMethod(self, &NotificationListener::accessDenied,
                                      Qt::QueuedConnection);
            co_return;
        }

        self->m_notificationChangedToken = listener.NotificationChanged(
            [self](const Listener &sender,
                   const UserNotificationChangedEventArgs &args) {
                if (!self) {
                    return;
                }

                try {
                    const QString id = QString::number(args.UserNotificationId());
                    if (args.ChangeKind() == UserNotificationChangedKind::Removed) {
                        QMetaObject::invokeMethod(
                            self,
                            [self, id] {
                                if (self) {
                                    emit self->notificationRemoved(id);
                                }
                            },
                            Qt::QueuedConnection);
                        return;
                    }

                    const ::Notification converted =
                        self->toNotification(sender.GetNotification(args.UserNotificationId()));
                    QMetaObject::invokeMethod(
                        self,
                        [self, converted] {
                            if (self) {
                                emit self->notificationAdded(converted);
                                emit self->notificationArrived(converted);
                            }
                        },
                        Qt::QueuedConnection);
                } catch (const winrt::hresult_error &error) {
                    if (self) {
                        const QString message = QString::fromWCharArray(error.message().c_str());
                        QMetaObject::invokeMethod(
                            self,
                            [self, message] {
                                if (self) {
                                    emit self->errorOccurred(message);
                                }
                            },
                            Qt::QueuedConnection);
                    }
                }
            });

        self->m_started = true;
        co_await self->loadNotifications(listener);
    } catch (const winrt::hresult_error &error) {
        if (self) {
            reportException(error);
        }
    }
}

winrt::Windows::Foundation::IAsyncAction NotificationListener::loadNotifications(Listener listener)
{
    QPointer<NotificationListener> self(this);
    const auto notifications =
        co_await listener.GetNotificationsAsync(NotificationKinds::Toast);

    for (const auto &notification : notifications) {
        if (!self) {
            co_return;
        }

        const ::Notification converted = toNotification(notification);
        QMetaObject::invokeMethod(
            self,
            [self, converted] {
                if (self) {
                    emit self->notificationAdded(converted);
                }
            },
            Qt::QueuedConnection);
    }
}

::Notification NotificationListener::toNotification(
    const UserNotification &userNotification) const
{
    ::Notification result;
    result.id = QString::number(userNotification.Id());
    result.timestamp = QDateTime::fromMSecsSinceEpoch(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            winrt::clock::to_sys(userNotification.CreationTime()).time_since_epoch())
            .count());

    const auto appInfo = userNotification.AppInfo();
    if (appInfo) {
        result.appName = QString::fromWCharArray(
            appInfo.DisplayInfo().DisplayName().c_str());
    }

    const auto binding = userNotification.Notification().Visual().GetBinding(
        KnownNotificationBindings::ToastGeneric());
    if (!binding) {
        return result;
    }

    QStringList text;
    for (const auto &element : binding.GetTextElements()) {
        text.append(QString::fromWCharArray(element.Text().c_str()));
    }

    if (!text.isEmpty()) {
        result.title = text.takeFirst();
        result.content = text.join('\n');
    }
    return result;
}

void NotificationListener::reportException(const winrt::hresult_error &error)
{
    const QString message = QString::fromWCharArray(error.message().c_str());
    QPointer<NotificationListener> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, message] {
            if (self) {
                emit self->errorOccurred(message);
            }
        },
        Qt::QueuedConnection);
}
