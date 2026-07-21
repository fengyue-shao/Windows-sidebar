#pragma once
#ifndef MEDIAWORKER_H
#define MEDIAWORKER_H
#include <winrt/Windows.Media.Control.h>
#include <QObject>
//struct Timeline;
class MediaWorker : public QObject
{
    Q_OBJECT
public:
    explicit MediaWorker(QObject *parent = nullptr);
public slots:
    void initialize();
signals:
    void mediaInfoUpdated(const QString& title,const QString& artist,const QString& album,
                          const QString& provider,const QImage& coverImage);
    void mediaPSUpdated(bool playing);
    //void mediaTimelineUpdated(Timeline timeline);
public slots:
    void togglePlayPause();
    void next();
    void previous();
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
    void updateCover(QImage& coverImage);
    static QString providerName(const Session &session);
    //void updateTimeline();
private:
    SessionManager m_manager{ nullptr };
    Session m_session{ nullptr };
    bool m_coverRetryPending = false;
    winrt::event_token m_sessionToken{};
    winrt::event_token m_mediaToken{};
    winrt::event_token m_playbackToken{};
    winrt::event_token m_timelineToken{};
};

#endif // MEDIAWORKER_H
