#include "include/mediamonitor.h"
#include "include/coverimageprovider.h"

#include <QMetaObject>

MediaMonitor::MediaMonitor(QObject *parent)
    : QObject(parent)
{
    m_worker = new MediaWorker;
    m_worker->moveToThread(&m_thread);
    connect(m_worker, &MediaWorker::controlsUpdated, this, [this](const QVariantMap &controls) {
        if (m_controls == controls) return;
        m_controls = controls; emit controlsChanged();
    });
    connect(m_worker, &MediaWorker::sessionsUpdated, this, [this](const QVariantList &sessions, const QString &selected, bool following) {
        if (sessions == m_sessions && selected == m_selectedId && following == m_followingSystem) return;
        m_sessions = sessions; m_selectedId = selected; m_followingSystem = following; emit sessionsChanged();
    });
    connect(m_worker, &MediaWorker::timelineUpdated, this, [this](double position, double duration, bool canSeek) {
        if (qFuzzyCompare(position+1,m_position+1) && qFuzzyCompare(duration+1,m_duration+1) && canSeek == m_canSeek) return;
        m_position = position; m_duration = duration; m_canSeek = canSeek; emit timelineChanged();
    });
    connect(m_worker, &MediaWorker::commandStatus, this, [this](const QString &status) {
        if (m_status == status) return; m_status = status; emit statusChanged();
    });

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
            m_title = title;
            m_artist = artist;
            m_album = album;
            m_provider = provider;
            if (m_coverProvider && m_coverProvider->setImage(coverImage)) {
                ++m_coverVersion;
                changed = true;
            }
            if (!changed)
                return;
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
    m_thread.requestInterruption();
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
    QMetaObject::invokeMethod(m_worker, &MediaWorker::refresh, Qt::QueuedConnection);
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
void MediaMonitor::selectSession(const QString &id) {
    QMetaObject::invokeMethod(m_worker, [worker=m_worker,id] { worker->selectSession(id); }, Qt::QueuedConnection);
}
void MediaMonitor::followSystemSession() {
    QMetaObject::invokeMethod(m_worker, &MediaWorker::followSystemSession, Qt::QueuedConnection);
}
void MediaMonitor::seekTo(double seconds, const QString &sessionId) {
    QMetaObject::invokeMethod(m_worker, [worker=m_worker,seconds,sessionId] { worker->seek(seconds,sessionId); }, Qt::QueuedConnection);
}
void MediaMonitor::invokeControl(const QString &actionId, const QString &sessionId) {
    QMetaObject::invokeMethod(m_worker, [worker=m_worker,actionId,sessionId] {
        worker->invokeControl(actionId,sessionId);
    }, Qt::QueuedConnection);
}
