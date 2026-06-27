#include "mediaworker.h"
#include <QDebug>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <winrt/base.h>
#include <winrt/windows.Foundation.h>
#include <winrt/windows.Foundation.collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
using namespace winrt;
using namespace Windows::Media::Control;
MediaWorker::MediaWorker(QObject *parent)
    : QObject{parent}
{
}
void MediaWorker::update(){
    try{
        auto session=m_session;
        if(!session){
            return;
        }
        auto media=session.TryGetMediaPropertiesAsync().get();
        QString coverPath;
        QString title=QString::fromWCharArray(media.Title().c_str());
        QString artist=QString::fromWCharArray(media.Artist().c_str());
        QString album=QString::fromWCharArray(media.AlbumTitle().c_str());
        bool playing=session.GetPlaybackInfo().PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
        QImage coverImage;
        auto thumbnail = media.Thumbnail();

        if (thumbnail)
        {
            try
            {
                auto streamRef = thumbnail;

                auto stream =
                    streamRef.OpenReadAsync().get();

                auto size =
                    static_cast<uint32_t>(stream.Size());

                Windows::Storage::Streams::Buffer buffer(size);

                stream.ReadAsync(
                          buffer,
                          size,
                          Windows::Storage::Streams::InputStreamOptions::None)
                    .get();

                auto reader =
                    Windows::Storage::Streams::DataReader::FromBuffer(buffer);

                QByteArray imageData;
                imageData.resize(size);

                reader.ReadBytes(
                    winrt::array_view<uint8_t>(
                        reinterpret_cast<uint8_t*>(imageData.data()),
                        reinterpret_cast<uint8_t*>(imageData.data() + size)));
                coverImage.loadFromData(imageData);
            }
            catch (...)
            {
            }
        }
        emit mediaUpdated(title,artist,album,playing,coverImage);
    }catch(...){
        qDebug()<<"GSMTC error";
    }
}
void MediaWorker::initialize()
{
    winrt::init_apartment(
        winrt::apartment_type::multi_threaded);

    m_manager =
        SessionManager::RequestAsync().get();

    m_sessionToken =
        m_manager.CurrentSessionChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    [this]
                    {
                        registerSession();
                    },
                    Qt::QueuedConnection);
            });

    registerSession();

    qDebug() << "MediaWorker initialized";
}
void MediaWorker::registerSession()
{
    unregisterSession();

    m_session = m_manager.GetCurrentSession();

    if (!m_session)
    {
        emit mediaUpdated(
            "",
            "",
            "",
            false,
            QImage());

        return;
    }

    m_mediaToken =
        m_session.MediaPropertiesChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::update,
                    Qt::QueuedConnection);
            });

    m_playbackToken =
        m_session.PlaybackInfoChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::update,
                    Qt::QueuedConnection);
            });

    m_timelineToken =
        m_session.TimelinePropertiesChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::update,
                    Qt::QueuedConnection);
            });

    update();
}
void MediaWorker::unregisterSession()
{
    if (!m_session)
        return;

    m_session.MediaPropertiesChanged(m_mediaToken);
    m_session.PlaybackInfoChanged(m_playbackToken);
    m_session.TimelinePropertiesChanged(m_timelineToken);

    m_session = nullptr;
}
