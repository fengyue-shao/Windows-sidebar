#include "include/mediamonitor.h"
#include "include/coverimageprovider.h"
#include <QDebug>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
using namespace winrt;
using namespace Windows::Media::Control;
MediaMonitor::MediaMonitor(QObject *parent)
    : QObject(parent)
{
    m_worker = new MediaWorker;

    m_worker->moveToThread(&m_thread);
    connect(
        m_worker,
        &MediaWorker::mediaInfoUpdated,
        this,
        [this](
            const QString& title,
            const QString& artist,
            const QString& album,
            const QImage& coverImage)
        {
            bool changed =
                title != m_title
                || artist != m_artist
                || album != m_album;
            if(!coverImage.isNull()){
                changed=true;
            }
            if (!changed)
                return;
            m_title = title;
            m_artist = artist;
            m_album = album;
            if(m_coverProvider)
            {
                m_coverProvider->setImage(
                    coverImage);
                m_coverVersion+=1;
            }
            emit mediaChanged();
        }, Qt::QueuedConnection);
    connect(
        m_worker,
        &MediaWorker::mediaPSUpdated,
        this,
        [this](
            bool playing)
        {
            bool changed =playing != m_playing;
            if (!changed)
                return;
            m_playing = playing;
            emit mediaChanged();
        });
    connect(
        &m_thread,
        &QThread::finished,
        m_worker,
        &QObject::deleteLater);

    connect(
        &m_thread,
        &QThread::started,
        m_worker,
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
void MediaMonitor::togglePlayPause()
{
    QMetaObject::invokeMethod(
        m_worker,
        &MediaWorker::togglePlayPause,
        Qt::QueuedConnection);
}
void MediaMonitor::next(){
    QMetaObject::invokeMethod(m_worker,&MediaWorker::next,Qt::QueuedConnection);
}
void MediaMonitor::previous(){
    QMetaObject::invokeMethod(m_worker,&MediaWorker::previous,Qt::QueuedConnection);
}