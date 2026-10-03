#include "include/notificationviewmodel.h"

NotificationViewModel::NotificationViewModel(NotificationModel *source, QObject *parent)
    : QSortFilterProxyModel(parent), m_notifications(source)
{
    setSourceModel(source);
    // Multiple roles participate in ordering/filtering; re-evaluate any role change.
    setSortRole(Qt::DisplayRole);
    setFilterRole(Qt::DisplayRole);
    setDynamicSortFilter(true);
    sort(0);
    connect(source, &QAbstractItemModel::dataChanged, this, [this] { invalidate(); });
    connect(this, &QAbstractItemModel::rowsInserted, this, &NotificationViewModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &NotificationViewModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &NotificationViewModel::countChanged);
}
void NotificationViewModel::setSearchText(const QString &text)
{
    if (m_searchText == text) return;
    m_searchText = text;
    invalidateFilter();
    emit filtersChanged();
}
void NotificationViewModel::setUnreadOnly(bool enabled)
{
    if (m_unreadOnly == enabled) return;
    m_unreadOnly = enabled;
    invalidateFilter();
    emit filtersChanged();
}
bool NotificationViewModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const auto idx = sourceModel()->index(row, 0, parent);
    if (m_unreadOnly && !idx.data(NotificationModel::UnreadRole).toBool()) return false;
    const auto query = m_searchText.trimmed();
    if (query.isEmpty()) return true;
    // Literal substring search, never user-supplied regular expressions or inferred categories.
    for (int role : {NotificationModel::AppNameRole, NotificationModel::TitleRole, NotificationModel::ContentRole})
        if (idx.data(role).toString().contains(query, Qt::CaseInsensitive)) return true;
    return false;
}
bool NotificationViewModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto leftGroup = left.data(NotificationModel::AppGroupRole).toString();
    const auto rightGroup = right.data(NotificationModel::AppGroupRole).toString();
    if (leftGroup != rightGroup) {
        return leftGroup < rightGroup;
    }
    const auto leftTime = left.data(NotificationModel::TimestampRole).toDateTime();
    const auto rightTime = right.data(NotificationModel::TimestampRole).toDateTime();
    if (leftTime != rightTime) return leftTime > rightTime;
    return left.data(NotificationModel::IdRole).toString() < right.data(NotificationModel::IdRole).toString();
}
int NotificationViewModel::sourceRowForId(const QString &id) const
{
    if (id.isEmpty()) return -1;
    for (int row = 0; row < m_notifications->rowCount(); ++row)
        if (m_notifications->index(row).data(NotificationModel::IdRole).toString() == id) return row;
    return -1;
}
void NotificationViewModel::removeNotificationById(const QString &id)
{
    const int row = sourceRowForId(id);
    if (row >= 0) m_notifications->removeNotification(row);
}
void NotificationViewModel::markReadById(const QString &id, bool read)
{
    const int row = sourceRowForId(id);
    if (row < 0) return;
    if (read) m_notifications->markAsRead(row);
    else m_notifications->markAsUnread(row);
}
QString NotificationViewModel::groupTitle(const QString &key) const
{
    for (int row = 0; row < rowCount(); ++row) {
        const auto idx = index(row, 0);
        if (idx.data(NotificationModel::AppGroupRole).toString() == key) {
            const auto name = idx.data(NotificationModel::AppNameRole).toString().trimmed();
            return name.isEmpty() ? QStringLiteral("未提供应用名称") : name;
        }
    }
    return {};
}

QStringList NotificationViewModel::visibleIds() const
{
    QStringList ids;
    for (int row = 0; row < rowCount(); ++row)
        ids.append(index(row, 0).data(NotificationModel::IdRole).toString());
    return ids;
}
void NotificationViewModel::markVisibleAsRead()
{
    const auto ids = visibleIds(); // Unread filtering changes immediately during this loop.
    for (const auto &id : ids) markReadById(id, true);
}
void NotificationViewModel::clearVisible()
{
    m_notifications->requestRemovalByIds(visibleIds());
}
