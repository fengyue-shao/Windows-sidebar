#pragma once
#ifndef NOTIFICATION_H
#define NOTIFICATION_H
#include <QDateTime>
#include <QUrl>
#include <QString>
enum class NotificationType
{
    System,
    Media,
    Message,
    Mail,
    Calendar,
    Download,
    Custom
};
struct Notification{
    QString id;
    QString appName;
    QString title;
    QString content;
    QUrl appIcon;
    QUrl image;
    QDateTime timestamp;
    bool unread=true;
    bool removable=true;
    NotificationType type=NotificationType::System;
};

Q_DECLARE_METATYPE(Notification)

#endif // NOTIFICATION_H
