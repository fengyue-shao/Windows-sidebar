#include "include/mediaworker.h"
#include <QDebug>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTimer>
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
void MediaWorker::updateCover(QImage &coverImage)
{
    try
    {
        auto media = m_session.TryGetMediaPropertiesAsync().get();
        auto thumbnail = media.Thumbnail();
        bool needRetry = false;

        if (!thumbnail)
        {
            needRetry = true;
        }
        else
        {
            auto stream = thumbnail.OpenReadAsync().get();
            if (!stream || stream.Size() == 0)
            {
                needRetry = true;
            }
            else
            {
                auto size = static_cast<uint32_t>(stream.Size());

                Windows::Storage::Streams::Buffer buffer(size);

                auto readBuffer =
                    stream.ReadAsync(
                              buffer,
                              size,
                              Windows::Storage::Streams::InputStreamOptions::None)
                        .get();
                if (readBuffer.Length() == 0)
                {
                    needRetry = true;
                }
                else
                {
                    auto reader =
                        Windows::Storage::Streams::DataReader::FromBuffer(readBuffer);

                    QByteArray imageData;
                    imageData.resize(readBuffer.Length());

                    reader.ReadBytes(
                        winrt::array_view<uint8_t>(
                            reinterpret_cast<uint8_t *>(imageData.data()),
                            reinterpret_cast<uint8_t *>(imageData.data() + imageData.size())));

                    if (!coverImage.loadFromData(imageData))
                    {
                        needRetry = true;
                    }
                }
            }
        }

        if (needRetry && !m_coverRetryPending)
        {
            m_coverRetryPending = true;
            qDebug()<<"retry to get img";
            QTimer::singleShot(1500, this, [this]()
                               {
                                   m_coverRetryPending = false;

                                   if (!m_session)
                                       return;

                                   updateInfo();
                               });
        }
    }
    catch (...)
    {
        if (!m_coverRetryPending)
        {
            m_coverRetryPending = true;

            QTimer::singleShot(200, this, [this]()
                               {
                                   m_coverRetryPending = false;

                                   if (!m_session)
                                       return;

                                   updateInfo();
                               });
        }
    }
}
void MediaWorker::updateInfo(){
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
        QImage coverImage;
        updateCover(coverImage);
        emit mediaInfoUpdated(title,artist,album,coverImage);
    }catch(...){
        qDebug()<<"GMSTC error";
    }
}
void MediaWorker::updatePS(){
    try{
        auto session=m_session;
        if(!session){
            return;
        }
        bool playing=session.GetPlaybackInfo().PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
        emit mediaPSUpdated(playing);
    }catch(...){
        qDebug()<<"GMSTC error";
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
        emit mediaInfoUpdated(
            "",
            "",
            "",
            QImage());
        emit mediaPSUpdated(false);
        return;
    }

    m_mediaToken =
        m_session.MediaPropertiesChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::updateInfo,
                    Qt::QueuedConnection);
            });
    updateInfo();
    m_playbackToken =
        m_session.PlaybackInfoChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::updatePS,
                    Qt::QueuedConnection);
            });
    updatePS();
    /*m_timelineToken =
        m_session.TimelinePropertiesChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::update,
                    Qt::QueuedConnection);
            });*/

    //update();
}
void MediaWorker::unregisterSession()
{
    if (!m_session)
        return;

    m_session.MediaPropertiesChanged(m_mediaToken);
    m_session.PlaybackInfoChanged(m_playbackToken);
    //m_session.TimelinePropertiesChanged(m_timelineToken);

    m_session = nullptr;
}
void MediaWorker::togglePlayPause()
{
    if (!m_session)
        return;

    try
    {
        m_session
            .TryTogglePlayPauseAsync()
            .get();
    }
    catch (...)
    {
        qDebug() << "Toggle failed";
    }
}
void MediaWorker::next()
{
    if (!m_session)
        return;

    try
    {
        m_session
            .TrySkipNextAsync()
            .get();
    }
    catch (...)
    {
    }
}
void MediaWorker::previous()
{
    if (!m_session)
        return;

    try
    {
        m_session
            .TrySkipPreviousAsync()
            .get();
    }
    catch (...)
    {
    }
}