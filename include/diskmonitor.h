#pragma once
#ifndef DISKMONITOR_H
#define DISKMONITOR_H

#include <QAbstractListModel>
#include <QVector>
#include "periodicmodule.h"

class DiskMonitor : public QAbstractListModel, public PeriodicModule
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY partitionsChanged)

public:
    explicit DiskMonitor(QObject *parent = nullptr);

    enum DiskRoles {
        RootPathRole = Qt::UserRole + 1,
        NameRole,
        FileSystemRole,
        TotalBytesRole,
        UsedBytesRole,
        AvailableBytesRole,
        UsageRole,
        TotalTextRole,
        UsedTextRole,
        AvailableTextRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;
    void refresh() override;

signals:
    void partitionsChanged();

private:
    struct Partition {
        QString rootPath;
        QString name;
        QString fileSystem;
        quint64 totalBytes = 0;
        quint64 usedBytes = 0;
        quint64 availableBytes = 0;
        double usage = 0.0;
    };

    void updateData();
    static QString formatBytes(quint64 bytes);
    static bool partitionsEqual(const QVector<Partition> &left,
                                const QVector<Partition> &right);

    QVector<Partition> m_partitions;
};

#endif // DISKMONITOR_H
