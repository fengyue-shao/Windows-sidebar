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
    Q_PROPERTY(QString provider READ provider NOTIFY mediaChanged FINAL)
    Q_PROPERTY(bool playing READ playing NOTIFY mediaChanged FINAL)

    Q_PROPERTY(int coverVersion READ coverVersion NOTIFY mediaChanged FINAL)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QString selectedSessionId READ selectedSessionId NOTIFY sessionsChanged)
    Q_PROPERTY(bool followingSystem READ followingSystem NOTIFY sessionsChanged)
    Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY timelineChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY timelineChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY timelineChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit MediaMonitor(QObject *parent = nullptr);
    ~MediaMonitor();

    QString artist() const;
    QString title() const;
    QString album() const;
    QString provider() const;
    bool playing() const;

    int coverVersion() const;
    QVariantList sessions() const { return m_sessions; }
    QString selectedSessionId() const { return m_selectedId; }
    bool followingSystem() const { return m_followingSystem; }
    double positionSeconds() const { return m_position; }
    double durationSeconds() const { return m_duration; }
    bool canSeek() const { return m_canSeek; }
    QString statusText() const { return m_status; }
    Q_INVOKABLE void selectSession(const QString &id);
    Q_INVOKABLE void followSystemSession();
    Q_INVOKABLE void seekTo(double seconds, const QString &sessionId);

    void setCoverProvider(
        CoverImageProvider* provider);

signals:
    void mediaChanged();
    void sessionsChanged();
    void timelineChanged();
    void statusChanged();
public slots:
    void togglePlayPause();
    void next();
    void previous();
private:
    QString m_artist;
    QString m_title;
    QString m_album;
    QString m_provider;
    bool m_playing = false;
    QThread m_thread;

    CoverImageProvider* m_coverProvider = nullptr;
    MediaWorker *m_worker=nullptr;
    int m_coverVersion = 0;
    QVariantList m_sessions;
    QString m_selectedId, m_status;
    bool m_followingSystem = true, m_canSeek = false;
    double m_position = 0, m_duration = 0;
};

#endif // MEDIAMONITOR_H
