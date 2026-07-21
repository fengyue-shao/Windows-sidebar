#include "include/diskmonitor.h"

#include <QDir>
#include <QStorageInfo>
#include <algorithm>

DiskMonitor::DiskMonitor(QObject *parent)
    : QAbstractListModel(parent)
{
    updateData();
}

int DiskMonitor::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_partitions.size();
}

QVariant DiskMonitor::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_partitions.size())
        return {};

    const Partition &partition = m_partitions.at(index.row());
    switch (role) {
    case RootPathRole:
        return partition.rootPath;
    case NameRole:
        return partition.name;
    case FileSystemRole:
        return partition.fileSystem;
    case TotalBytesRole:
        return QVariant::fromValue(partition.totalBytes);
    case UsedBytesRole:
        return QVariant::fromValue(partition.usedBytes);
    case AvailableBytesRole:
        return QVariant::fromValue(partition.availableBytes);
    case UsageRole:
        return partition.usage;
    case TotalTextRole:
        return formatBytes(partition.totalBytes);
    case UsedTextRole:
        return formatBytes(partition.usedBytes);
    case AvailableTextRole:
        return formatBytes(partition.availableBytes);
    default:
        return {};
    }
}

QHash<int, QByteArray> DiskMonitor::roleNames() const
{
    return {
        {RootPathRole, "rootPath"},
        {NameRole, "name"},
        {FileSystemRole, "fileSystem"},
        {TotalBytesRole, "totalBytes"},
        {UsedBytesRole, "usedBytes"},
        {AvailableBytesRole, "availableBytes"},
        {UsageRole, "usage"},
        {TotalTextRole, "totalText"},
        {UsedTextRole, "usedText"},
        {AvailableTextRole, "availableText"}
    };
}

int DiskMonitor::count() const
{
    return m_partitions.size();
}

void DiskMonitor::refresh()
{
    updateData();
}

void DiskMonitor::updateData()
{
    QVector<Partition> partitions;
    const auto volumes = QStorageInfo::mountedVolumes();

    for (const QStorageInfo &volume : volumes) {
        if (!volume.isValid() || !volume.isReady() || volume.bytesTotal() <= 0)
            continue;

        const QString rootPath = QDir::toNativeSeparators(volume.rootPath());
        if (rootPath.size() < 2 || rootPath.at(1) != ':')
            continue;

        Partition partition;
        partition.rootPath = rootPath;
        partition.name = volume.displayName().isEmpty() ? rootPath : volume.displayName();
        partition.fileSystem = QString::fromUtf8(volume.fileSystemType());
        partition.totalBytes = static_cast<quint64>(volume.bytesTotal());
        partition.availableBytes = static_cast<quint64>(volume.bytesAvailable());
        partition.usedBytes = partition.totalBytes - partition.availableBytes;
        partition.usage = static_cast<double>(partition.usedBytes) * 100.0
                          / static_cast<double>(partition.totalBytes);
        partitions.append(partition);
    }

    std::sort(partitions.begin(), partitions.end(),
              [](const Partition &left, const Partition &right) {
                  return left.rootPath < right.rootPath;
              });

    if (partitionsEqual(partitions, m_partitions))
        return;

    beginResetModel();
    m_partitions = std::move(partitions);
    endResetModel();
    emit partitionsChanged();
}

QString DiskMonitor::formatBytes(quint64 bytes)
{
    constexpr double gigabyte = 1024.0 * 1024.0 * 1024.0;
    constexpr double terabyte = gigabyte * 1024.0;
    if (bytes >= terabyte)
        return QString("%1 TB").arg(bytes / terabyte, 0, 'f', 1);
    return QString("%1 GB").arg(bytes / gigabyte, 0, 'f', 1);
}

bool DiskMonitor::partitionsEqual(const QVector<Partition> &left,
                                  const QVector<Partition> &right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype i = 0; i < left.size(); ++i) {
        if (left.at(i).rootPath != right.at(i).rootPath
            || left.at(i).totalBytes != right.at(i).totalBytes
            || left.at(i).availableBytes != right.at(i).availableBytes)
            return false;
    }
    return true;
}
