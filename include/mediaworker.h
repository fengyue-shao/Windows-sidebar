#pragma once
#ifndef MEDIAWORKER_H
#define MEDIAWORKER_H
#include <winrt/Windows.Media.Control.h>
#include <QObject>
#include <QImage>
#include <QVariantList>
#include <QTimer>
#include <vector>
//struct Timeline;
class MediaWorker : public QObject
{
    Q_OBJECT
public:
    explicit MediaWorker(QObject *parent = nullptr);
    ~MediaWorker() override;
public slots:
    void initialize();
signals:
    void mediaInfoUpdated(const QString& title,const QString& artist,const QString& album,
                          const QString& provider,const QImage& coverImage);
    void mediaPSUpdated(bool playing);
    void sessionsUpdated(const QVariantList &sessions, const QString &selectedId, bool followingSystem);
    void timelineUpdated(double position, double duration, bool canSeek);
    void commandStatus(const QString &message);
    //void mediaTimelineUpdated(Timeline timeline);
public slots:
    void togglePlayPause();
    void next();
    void previous();
    void refresh();
    void selectSession(const QString &id);
    void followSystemSession();
    void seek(double seconds, const QString &sessionId);
private:
    using SessionManager =
        winrt::Windows::Media::Control::
        GlobalSystemMediaTransportControlsSessionManager;

    using Session =
        winrt::Windows::Media::Control::
        GlobalSystemMediaTransportControlsSession;

    void registerSession();
    void unregisterSession();
    void updateInfo();
    void updatePS();
    void updateTimeline();
    void refreshSessions();
    QImage readCover(const winrt::Windows::Media::Control::
                         GlobalSystemMediaTransportControlsSessionMediaProperties &media,
                     bool &needsRetry);
    void scheduleCoverRetry(const QString &mediaKey);
    static QString providerName(const Session &session);
    //void updateTimeline();
private:
    SessionManager m_manager{ nullptr };
    Session m_session{ nullptr };
    struct Entry { QString id, name; Session session{nullptr}; };
    std::vector<Entry> m_sessions;
    QString m_selectedId;
    bool m_followSystem = true;
    int m_nextId = 1;
    QTimer *m_pollTimer = nullptr;
    bool m_coverRetryPending = false;
    int m_coverRetryAttempt = 0;
    QString m_mediaKey;
    winrt::event_token m_sessionToken{};
    winrt::event_token m_sessionsToken{};
    winrt::event_token m_mediaToken{};
    winrt::event_token m_playbackToken{};
    winrt::event_token m_timelineToken{};
};

#endif // MEDIAWORKER_H
