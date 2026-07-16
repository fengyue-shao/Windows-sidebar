#pragma once
#ifndef MEDIAMONITOR_H
#define MEDIAMONITOR_H
#include <QObject>
#include <QThread>
#include "mediaworker.h"
class CoverImageProvider;

class MediaMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString artist READ artist NOTIFY mediaChanged FINAL)
    Q_PROPERTY(QString title READ title NOTIFY mediaChanged FINAL)
    Q_PROPERTY(QString album READ album NOTIFY mediaChanged FINAL)
    Q_PROPERTY(bool playing READ playing NOTIFY mediaChanged FINAL)

    Q_PROPERTY(int coverVersion READ coverVersion NOTIFY mediaChanged FINAL)

public:
    explicit MediaMonitor(QObject *parent = nullptr);
    ~MediaMonitor();

    QString artist() const;
    QString title() const;
    QString album() const;
    bool playing() const;

    int coverVersion() const;

    void setCoverProvider(
        CoverImageProvider* provider);

signals:
    void mediaChanged();
public slots:
    void togglePlayPause();
    void next();
    void previous();
private:
    QString m_artist;
    QString m_title;
    QString m_album;
    bool m_playing = false;
    QThread m_thread;

    CoverImageProvider* m_coverProvider = nullptr;
    MediaWorker *m_worker=nullptr;
    int m_coverVersion = 0;
};

#endif // MEDIAMONITOR_H
