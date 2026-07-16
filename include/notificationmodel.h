#ifndef NOTIFICATIONMODEL_H
#define NOTIFICATIONMODEL_H
#include <QVector>
#include <QAbstractListModel>
#include "Notification.h"
class NotificationModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit NotificationModel(QObject *parent=nullptr);
    enum NotificationRoles{
        IdRole=Qt::UserRole+1,
        AppNameRole,
        TitleRole,
        ContentRole,
        AppIconRole,
        ImageRole,
        TimestampRole,
        UnreadRole,
        RemovableRole,
        TypeRole
    };
    int rowCount(const QModelIndex &parent=QModelIndex())const override;
    QVariant data(const QModelIndex &index,int role=Qt::DisplayRole)const override;
    QHash<int,QByteArray> roleNames() const override;
    Q_INVOKABLE void addNotification(const Notification &notification);
    Notification notification(int row)const;
    Q_INVOKABLE void removeNotification(int row);
    Q_INVOKABLE void removeById(const QString &id);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void markAsRead(int row);
    Q_INVOKABLE void markAsUnread(int row);
    Q_INVOKABLE void markAllAsRead();
    Q_INVOKABLE int unreadCount()const;
    bool contains(const QString &id)const;

private:
    QVector<Notification>m_notifications;
};

#endif // NOTIFICATIONMODEL_H
