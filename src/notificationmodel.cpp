#include "include/notificationmodel.h"

NotificationModel::NotificationModel(QObject *parent):QAbstractListModel(parent) {

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
        {TypeRole, "type"}
    };
}

void NotificationModel::addNotification(const Notification &notification)
{
    beginInsertRows(QModelIndex(),
                    m_notifications.size(),
                    m_notifications.size());

    m_notifications.append(notification);

    endInsertRows();
}

void NotificationModel::removeNotification(int row)
{
    if (row < 0 || row >= m_notifications.size())
        return;

    beginRemoveRows(QModelIndex(), row, row);

    m_notifications.removeAt(row);

    endRemoveRows();
}
void NotificationModel::removeById(const QString &id)
{
    for (int i = 0; i < m_notifications.size(); ++i)
    {
        if (m_notifications[i].id == id)
        {
            removeNotification(i);
            return;
        }
    }
}
void NotificationModel::clear()
{
    beginResetModel();

    m_notifications.clear();

    endResetModel();
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
}
void NotificationModel::markAllAsRead()
{
    if (m_notifications.isEmpty())
        return;
    for (int i = 0; i < m_notifications.size(); ++i)
    {
        m_notifications[i].unread = false;
    }

    emit dataChanged(
        index(0),
        index(m_notifications.size() - 1),
        {UnreadRole});
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