#pragma once
#include <QSortFilterProxyModel>
#include "notificationmodel.h"

// Filtering is presentation-only. Commands resolve an immutable notification ID
// against the current source, never a cached filtered row number.
class NotificationViewModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filtersChanged)
    Q_PROPERTY(bool unreadOnly READ unreadOnly WRITE setUnreadOnly NOTIFY filtersChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool filtered READ filtered NOTIFY filtersChanged)
public:
    explicit NotificationViewModel(NotificationModel *source, QObject *parent = nullptr);
    QString searchText() const { return m_searchText; }
    bool unreadOnly() const { return m_unreadOnly; }
    bool filtered() const { return m_unreadOnly || !m_searchText.trimmed().isEmpty(); }
    int count() const { return rowCount(); }
    void setSearchText(const QString &text);
    void setUnreadOnly(bool enabled);
    Q_INVOKABLE void removeNotificationById(const QString &id);
    Q_INVOKABLE void markReadById(const QString &id, bool read);
    Q_INVOKABLE QString groupTitle(const QString &key) const;
    Q_INVOKABLE void markVisibleAsRead();
    Q_INVOKABLE void clearVisible();
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
signals:
    void filtersChanged();
    void countChanged();
private:
    int sourceRowForId(const QString &id) const;
    QStringList visibleIds() const;
    NotificationModel *m_notifications;
    QString m_searchText;
    bool m_unreadOnly = false;
};
