#include "include/notificationmodel.h"
#include <QSet>
//#include "include/notificationlistener.h"
NotificationModel::NotificationModel(QObject *parent):QAbstractListModel(parent) {
  //  connect(&NotificationListener,&NotificationListener::notificationAdded,);
}
int NotificationModel::rowCount(const QModelIndex &parent)const{
    if(parent.isValid())
        return 0;
    return m_notifications.size();
}
QVariant NotificationModel::data(const QModelIndex &index,int role)const{
    if(!index.isValid())
        return {};
    if(index.row()<0||index.row()>=m_notifications.size())
        return {};
    const Notification &notification=m_notifications.at(index.row());
    switch(role){
    case IdRole:
        return notification.id;
    case AppNameRole:
        return notification.appName;
    case AppIdRole:
        return notification.appId;
    case AppGroupRole:
        return notification.appId.isEmpty() ? QStringLiteral("name:") + notification.appName : QStringLiteral("id:") + notification.appId;
    case TitleRole:
        return notification.title;
    case ContentRole:
        return notification.content;

    case AppIconRole:
        return notification.appIcon;

    case ImageRole:
        return notification.image;

    case TimestampRole:
        return notification.timestamp;

    case UnreadRole:
        return notification.unread;

    case RemovableRole:
        return notification.removable;

    case TypeRole:
        return static_cast<int>(notification.type);

    default:
        return {};
    }
}
QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {AppNameRole, "appName"},
        {TitleRole, "title"},
        {ContentRole, "content"},
        {AppIconRole, "appIcon"},
        {ImageRole, "image"},
        {TimestampRole, "timestamp"},
        {UnreadRole, "unread"},
        {RemovableRole, "removable"},
        {TypeRole, "type"},
        {AppIdRole, "appId"},
        {AppGroupRole, "appGroup"}
    };
}

void NotificationModel::addNotification(const Notification &notification)
{
    for (int row = 0; row < m_notifications.size(); ++row) {
        if (!notification.id.isEmpty() && m_notifications.at(row).id == notification.id) {
            const bool unread = m_notifications.at(row).unread;
            m_notifications[row] = notification;
            m_notifications[row].unread = unread;
            emit dataChanged(index(row), index(row));
            return;
        }
    }

    beginInsertRows(QModelIndex(),
                    m_notifications.size(),
                    m_notifications.size());

    m_notifications.append(notification);

    endInsertRows();
    if (notification.unread)
        emit unreadCountChanged();
}

void NotificationModel::removeNotification(int row)
{
    if (row < 0 || row >= m_notifications.size())
        return;

    const QString id = m_notifications.at(row).id;
    if (!id.isEmpty() && m_notifications.at(row).removable)
        emit removalRequested(id);
}
void NotificationModel::removeById(const QString &id)
{
    for (int i = 0; i < m_notifications.size(); ++i)
    {
        if (m_notifications[i].id == id)
        {
            const bool wasUnread = m_notifications.at(i).unread;
            beginRemoveRows(QModelIndex(), i, i);
            m_notifications.removeAt(i);
            endRemoveRows();
            if (wasUnread)
                emit unreadCountChanged();
            return;
        }
    }
}
void NotificationModel::clear()
{
    if (m_notifications.isEmpty())
        return;

    emit clearRequested();
}

void NotificationModel::synchronize(const QVector<Notification> &notifications)
{
    QHash<QString, bool> previousUnread;
    for (const auto &item : m_notifications)
        previousUnread.insert(item.id, item.unread);
    const int oldUnread = unreadCount();
    beginResetModel();
    m_notifications.clear();
    QSet<QString> seen;
    for (auto item : notifications) {
        if (item.id.isEmpty() || seen.contains(item.id)) continue;
        seen.insert(item.id);
        if (previousUnread.contains(item.id)) item.unread = previousUnread.value(item.id);
        m_notifications.append(item);
    }
    endResetModel();
    if (oldUnread != unreadCount()) emit unreadCountChanged();
}

void NotificationModel::requestRemovalByIds(const QStringList &ids)
{
    const QSet<QString> wanted(ids.cbegin(), ids.cend());
    QStringList targets;
    for (const auto &notice : m_notifications)
        if (!notice.id.isEmpty() && notice.removable && wanted.contains(notice.id) && !targets.contains(notice.id))
            targets.append(notice.id);
    if (!targets.isEmpty()) emit removalsRequested(targets);
}

Notification NotificationModel::notification(int row) const
{
    return m_notifications.at(row);
}
void NotificationModel::markAsRead(int row)
{
    if (row < 0 || row >= m_notifications.size())
        return;

    if (!m_notifications[row].unread)
        return;

    m_notifications[row].unread = false;

    emit dataChanged(
        index(row),
        index(row),
        {UnreadRole});
    emit unreadCountChanged();
}
void NotificationModel::markAsUnread(int row)
{
    if (row < 0 || row >= m_notifications.size())
        return;

    if (m_notifications[row].unread)
        return;

    m_notifications[row].unread = true;

    emit dataChanged(
        index(row),
        index(row),
        {UnreadRole});
    emit unreadCountChanged();
}
void NotificationModel::markAllAsRead()
{
    if (m_notifications.isEmpty())
        return;
    bool changed = false;
    for (int i = 0; i < m_notifications.size(); ++i)
    {
        if (m_notifications[i].unread) {
            m_notifications[i].unread = false;
            changed = true;
        }
    }

    if (!changed)
        return;

    emit dataChanged(
        index(0),
        index(m_notifications.size() - 1),
        {UnreadRole});
    emit unreadCountChanged();
}
int NotificationModel::unreadCount() const
{
    int count = 0;

    for (const auto &notification : m_notifications)
    {
        if (notification.unread)
            ++count;
    }

    return count;
}
bool NotificationModel::contains(const QString &id) const
{
    for (const Notification &notification : m_notifications)
    {
        if (notification.id == id)
            return true;
    }

    return false;
}
