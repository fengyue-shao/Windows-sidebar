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
    stop();
}

void NotificationListener::start()
{
    if (m_started || m_starting) return;
    m_session = std::make_shared<std::atomic_bool>(true);
    m_starting = true;
    setStatus(QStringLiteral("正在请求通知访问权限…"));
    try {
        m_listener = Listener::Current();
        // RequestAccessAsync must originate on the application UI thread.
        m_permission = m_listener.RequestAccessAsync();
        awaitPermission(this, m_session, m_permission);
    } catch (const winrt::hresult_error &error) {
        m_starting = false;
        reportException(error);
    }
}

void NotificationListener::stop()
{
    if (m_session) m_session->store(false);
    // Cancel is best effort: the session gate also rejects late completions.
    try { if (m_permission) m_permission.Cancel(); } catch (...) {}
    try { if (m_read) m_read.Cancel(); } catch (...) {}
    try { if (m_listener && m_started) m_listener.NotificationChanged(m_notificationChangedToken); } catch (...) {}
    m_permission = nullptr; m_read = nullptr; m_listener = nullptr;
    m_started = m_starting = m_reading = m_clearPending = false;
    m_pendingRemoval.clear();
    m_deleteErrors.clear();
    setStatus(QStringLiteral("通知模块已关闭；已有列表保留，重新开启后同步"));
}

void NotificationListener::setStatus(const QString &message, bool failed)
{
    m_statusText = message;
    m_failed = failed;
    emit statusChanged();
}

void NotificationListener::removeNotification(const QString &id)
{
    removeNotifications({id});
}

void NotificationListener::removeNotifications(const QStringList &ids)
{
    if (!m_started || busy()) {
        setStatus(QStringLiteral("通知监听未就绪或正在同步，未删除通知"), true);
        return;
    }

    if (ids.isEmpty()) return;
    m_deleteErrors.clear();
    const QSet<QString> uniqueIds(ids.cbegin(), ids.cend());
    for (const auto &id : uniqueIds) {
        bool ok = false;
        const uint notificationId = id.toUInt(&ok);
        if (!ok) {
            m_deleteErrors.append(QStringLiteral("通知标识无效：") + id);
            continue;
        }
        try {
            m_listener.RemoveNotification(notificationId);
            ++m_revision;
            m_pendingRemoval.insert(id);
        } catch (const winrt::hresult_error &error) {
            m_deleteErrors.append(id + QStringLiteral("：") + QString::fromWCharArray(error.message().c_str()));
        }
    }
    if (!m_deleteErrors.isEmpty()) emit errorOccurred(m_deleteErrors.join(QStringLiteral("；")));
    synchronize();
}

void NotificationListener::clearNotifications()
{
    if (!m_started || busy()) {
        setStatus(QStringLiteral("通知监听未就绪或正在同步，未清空通知"), true);
        return;
    }

    try {
        m_listener.ClearNotifications();
        ++m_revision;
        m_clearPending = true;
        synchronize();
    } catch (const winrt::hresult_error &error) {
        reportException(error);
    }
}

winrt::fire_and_forget NotificationListener::awaitPermission(QPointer<NotificationListener> self, Session session, AccessOperation operation)
{
    try {
        const auto access = co_await operation;
        if (!self || !session->load()) co_return;
        QMetaObject::invokeMethod(self, [self, session, access] {
            if (!self || !self->current(session)) return;
            self->m_starting = false;
            self->m_permission = nullptr;
            if (access != UserNotificationListenerAccessStatus::Allowed) {
                self->setStatus(QStringLiteral("未获得通知访问权限；可在 Windows 设置中允许访问后重试"), true);
                emit self->accessDenied();
                return;
            }
            self->activate(session);
        }, Qt::QueuedConnection);
    } catch (const winrt::hresult_error &error) {
        if (!self || !session->load()) co_return;
        const auto message = QString::fromWCharArray(error.message().c_str());
        QMetaObject::invokeMethod(self, [self, session, message] {
            if (!self || !self->current(session)) return;
            self->m_starting = false; self->m_permission = nullptr;
            self->setStatus(QStringLiteral("通知授权失败：") + message, true);
            emit self->errorOccurred(message);
        }, Qt::QueuedConnection);
    }
}

void NotificationListener::activate(Session session)
{
    QPointer<NotificationListener> self(this);
    try {
        m_notificationChangedToken = m_listener.NotificationChanged(
            [self, session](const Listener &sender, const UserNotificationChangedEventArgs &args) {
                if (!self || !session->load()) return;
                const auto id = args.UserNotificationId();
                const bool removed = args.ChangeKind() == UserNotificationChangedKind::Removed;
                // Read system state only after the session is checked on the Qt UI thread.
                QMetaObject::invokeMethod(self, [self, session, sender, id, removed] {
                    if (self && self->current(session)) self->changed(session, sender, id, removed);
                }, Qt::QueuedConnection);
            });
        m_started = true;
        synchronize();
    } catch (const winrt::hresult_error &error) { reportException(error); }
}

void NotificationListener::changed(Session session, Listener listener, uint id, bool removed)
{
    if (!current(session)) return;
    ++m_revision;
    if (removed) {
        emit notificationRemoved(QString::number(id));
    } else {
        try {
            const auto notification = listener.GetNotification(id);
            if (notification) {
                const auto converted = toNotification(notification);
                emit notificationAdded(converted);
                emit notificationArrived(converted);
            }
        } catch (const winrt::hresult_error &error) { reportException(error); }
    }
    if (!m_reading) synchronize();
}

void NotificationListener::synchronize()
{
    if (!m_started || m_reading) return;
    try {
        if (m_listener.GetAccessStatus() != UserNotificationListenerAccessStatus::Allowed) {
            stop();
            setStatus(QStringLiteral("通知访问权限已撤销；已有列表保留"), true);
            emit accessDenied();
            return;
        }
        m_reading = true;
        setStatus(QStringLiteral("正在同步 Windows 通知…"));
        m_read = m_listener.GetNotificationsAsync(NotificationKinds::Toast);
        awaitSnapshot(this, m_session, m_read, m_revision);
    } catch (const winrt::hresult_error &error) {
        m_reading = false;
        reportException(error);
    }
}

winrt::fire_and_forget NotificationListener::awaitSnapshot(QPointer<NotificationListener> self, Session session, ReadOperation operation, quint64 revision)
{
    try {
        const auto notifications = co_await operation;
        if (!self || !session->load()) co_return;
        QVector<::Notification> snapshot;
        for (const auto &notification : notifications) {
            if (!session->load()) co_return;
            snapshot.append(toNotification(notification));
        }
        QMetaObject::invokeMethod(self, [self, session, snapshot, revision] {
            if (!self || !self->current(session)) return;
            self->m_reading = false; self->m_read = nullptr;
            // Permission can be revoked while the read is in flight; an empty
            // denied result must not be mistaken for a confirmed empty list.
            try {
                if (self->m_listener.GetAccessStatus() != UserNotificationListenerAccessStatus::Allowed) {
                    self->stop();
                    self->setStatus(QStringLiteral("通知访问权限已撤销；已有列表保留"), true);
                    emit self->accessDenied();
                    return;
                }
            } catch (const winrt::hresult_error &error) { self->reportException(error); return; }
            // A concurrent change invalidates the snapshot: do not resurrect deleted entries.
            if (revision != self->m_revision) { self->synchronize(); return; }
            QSet<QString> ids;
            for (const auto &notice : snapshot) ids.insert(notice.id);
            bool pending = self->m_clearPending && !ids.isEmpty();
            for (const auto &id : self->m_pendingRemoval) pending |= ids.contains(id);
            self->m_clearPending = false; self->m_pendingRemoval.clear();
            emit self->snapshotReady(snapshot);
            QString message = pending ? QStringLiteral("系统列表中仍有待删除通知，尚未确认全部删除；可刷新重试")
                                      : QStringLiteral("已同步 Windows 通知");
            const bool failed = pending || !self->m_deleteErrors.isEmpty();
            if (!self->m_deleteErrors.isEmpty()) message += QStringLiteral("；部分删除失败：") + self->m_deleteErrors.join(QStringLiteral("；"));
            self->m_deleteErrors.clear();
            self->setStatus(message, failed);
        }, Qt::QueuedConnection);
    } catch (const winrt::hresult_error &error) {
        if (!self || !session->load()) co_return;
        const auto message = QString::fromWCharArray(error.message().c_str());
        QMetaObject::invokeMethod(self, [self, session, message] {
            if (!self || !self->current(session)) return;
            self->m_reading = false; self->m_read = nullptr;
            self->m_clearPending = false; self->m_pendingRemoval.clear();
            self->m_deleteErrors.clear();
            self->setStatus(QStringLiteral("通知同步失败，保留现有列表：") + message, true);
            emit self->errorOccurred(message);
        }, Qt::QueuedConnection);
    }
}

::Notification NotificationListener::toNotification(
    const UserNotification &userNotification)
{
    ::Notification result;
    result.id = QString::number(userNotification.Id());
    result.timestamp = QDateTime::fromMSecsSinceEpoch(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            winrt::clock::to_sys(userNotification.CreationTime()).time_since_epoch())
            .count());

    const auto appInfo = userNotification.AppInfo();
    if (appInfo) {
        result.appId = QString::fromWCharArray(appInfo.AppUserModelId().c_str());
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
    setStatus(QStringLiteral("通知操作失败，保留未确认项目：") + message, true);
    emit errorOccurred(message);
}
