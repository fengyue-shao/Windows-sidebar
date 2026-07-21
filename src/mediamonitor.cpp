#include "include/mediamonitor.h"
#include "include/coverimageprovider.h"

#include <QMetaObject>

MediaMonitor::MediaMonitor(QObject *parent)
    : QObject(parent)
{
    m_worker = new MediaWorker;
    m_worker->moveToThread(&m_thread);

    connect(
        m_worker,
        &MediaWorker::mediaInfoUpdated,
        this,
        [this](const QString &title,
               const QString &artist,
               const QString &album,
               const QString &provider,
               const QImage &coverImage)
        {
            bool changed = title != m_title
                           || artist != m_artist
                           || album != m_album
                           || provider != m_provider;
            if (!coverImage.isNull())
                changed = true;
            if (!changed)
                return;

            m_title = title;
            m_artist = artist;
            m_album = album;
            m_provider = provider;
            if (m_coverProvider && !coverImage.isNull()) {
                m_coverProvider->setImage(coverImage);
                ++m_coverVersion;
            }
            emit mediaChanged();
        },
        Qt::QueuedConnection);

    connect(
        m_worker,
        &MediaWorker::mediaPSUpdated,
        this,
        [this](bool playing)
        {
            if (playing == m_playing)
                return;
            m_playing = playing;
            emit mediaChanged();
        },
        Qt::QueuedConnection);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::started, m_worker, &MediaWorker::initialize);
    m_thread.start();
}

MediaMonitor::~MediaMonitor()
{
    m_thread.quit();
    m_thread.wait();
}

QString MediaMonitor::title() const { return m_title; }
QString MediaMonitor::artist() const { return m_artist; }
QString MediaMonitor::album() const { return m_album; }
QString MediaMonitor::provider() const { return m_provider; }
bool MediaMonitor::playing() const { return m_playing; }
int MediaMonitor::coverVersion() const { return m_coverVersion; }

void MediaMonitor::setCoverProvider(CoverImageProvider *provider)
{
    m_coverProvider = provider;
}

void MediaMonitor::togglePlayPause()
{
    QMetaObject::invokeMethod(m_worker, &MediaWorker::togglePlayPause, Qt::QueuedConnection);
}

void MediaMonitor::next()
{
    QMetaObject::invokeMethod(m_worker, &MediaWorker::next, Qt::QueuedConnection);
}

void MediaMonitor::previous()
{
    QMetaObject::invokeMethod(m_worker, &MediaWorker::previous, Qt::QueuedConnection);
}
