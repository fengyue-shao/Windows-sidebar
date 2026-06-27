#pragma once
#ifndef MEDIAWORKER_H
#define MEDIAWORKER_H
#include <winrt/Windows.Media.Control.h>
#include <QObject>

class MediaWorker : public QObject
{
    Q_OBJECT
public:
    explicit MediaWorker(QObject *parent = nullptr);
public slots:
    void initialize();
signals:
    void mediaUpdated(const QString& title,const QString& artist,const QString& album,bool playing,const QImage& coverImage);
private:
    void registerSession();
    void unregisterSession();
    void update();
private:
    using SessionManager =
        winrt::Windows::Media::Control::
        GlobalSystemMediaTransportControlsSessionManager;

    using Session =
        winrt::Windows::Media::Control::
        GlobalSystemMediaTransportControlsSession;

    SessionManager m_manager{ nullptr };
    Session m_session{ nullptr };

    winrt::event_token m_sessionToken{};
    winrt::event_token m_mediaToken{};
    winrt::event_token m_playbackToken{};
    winrt::event_token m_timelineToken{};
};

#endif // MEDIAWORKER_H
