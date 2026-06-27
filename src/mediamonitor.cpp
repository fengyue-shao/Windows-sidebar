#include "mediamonitor.h"
#include "mediaworker.h"
#include "coverimageprovider.h"
#include <QDebug>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
using namespace winrt;
using namespace Windows::Media::Control;
MediaMonitor::MediaMonitor(QObject *parent)
    : QObject(parent)
{
    auto worker = new MediaWorker;

    worker->moveToThread(&m_thread);

    connect(
        worker,
        &MediaWorker::mediaUpdated,
        this,
        [this](
            const QString& title,
            const QString& artist,
            const QString& album,
            bool playing,
            const QImage& coverImage)
        {
            bool changed =
                title != m_title
                || artist != m_artist
                || album != m_album
                           || playing != m_playing;


            if (!changed)
                return;

            m_title = title;
            m_artist = artist;
            m_album = album;
            m_playing = playing;

            if(m_coverProvider)
            {
                m_coverProvider->setImage(
                    coverImage);
                ++m_coverVersion;
            }
            emit mediaChanged();
        });

    connect(
        &m_thread,
        &QThread::finished,
        worker,
        &QObject::deleteLater);

    connect(
        &m_thread,
        &QThread::started,
        worker,
        &MediaWorker::initialize);

    m_thread.start();

}
MediaMonitor::~MediaMonitor(){
    m_thread.quit();
    m_thread.wait();
}
QString MediaMonitor::title() const{
    return m_title;
}
QString MediaMonitor::artist() const{
    return m_artist;
}
QString MediaMonitor::album() const{
    return m_album;
}
bool MediaMonitor::playing() const{
    return m_playing;
}
void MediaMonitor::setCoverProvider(CoverImageProvider* provider){
    m_coverProvider=provider;
};
int MediaMonitor::coverVersion() const{
    return m_coverVersion;
}