#include "include/mediaworker.h"
#include "include/boundedasync.h"
#include <QDebug>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <QThread>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/windows.Foundation.h>
#include <winrt/windows.Foundation.collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
using namespace winrt;
using namespace Windows::Media::Control;
namespace {
// wait_for is the single waiter; never call get() after it. Cancellation is
// best effort, but we stop waiting even when a player does not acknowledge it.
template<typename Operation> auto boundedResult(Operation operation)
{
    return SidebarAsync::result(operation, QThread::currentThread()->isInterruptionRequested());
}
}
MediaWorker::MediaWorker(QObject *parent)
    : QObject{parent}
{
}
QImage MediaWorker::readCover(
    const GlobalSystemMediaTransportControlsSessionMediaProperties &media,
    bool &needsRetry)
{
    needsRetry = false;
    QImage coverImage;
    try
    {
        auto thumbnail = media.Thumbnail();

        if (!thumbnail)
        {
            needsRetry = true;
        }
        else
        {
            auto stream = boundedResult(thumbnail.OpenReadAsync());
            if (!stream || stream.Size() == 0)
            {
                needsRetry = true;
            }
            else
            {
                if (stream.Size() > 12 * 1024 * 1024) return {};
                auto size = static_cast<uint32_t>(stream.Size());

                Windows::Storage::Streams::Buffer buffer(size);

                auto readBuffer =
                    boundedResult(stream.ReadAsync(
                              buffer,
                              size,
                              Windows::Storage::Streams::InputStreamOptions::None));
                if (readBuffer.Length() == 0)
                {
                    needsRetry = true;
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
                        needsRetry = true;
                    }
                }
            }
        }

    }
    catch (...)
    {
        needsRetry = true;
    }
    return coverImage;
}
MediaWorker::~MediaWorker() {
    try {
        unregisterSession();
        if (m_manager) { m_manager.CurrentSessionChanged(m_sessionToken); m_manager.SessionsChanged(m_sessionsToken); }
    } catch (...) {}
}

void MediaWorker::scheduleCoverRetry(const QString &mediaKey)
{
    if (m_coverRetryPending || m_coverRetryAttempt >= 4)
        return;

    m_coverRetryPending = true;
    ++m_coverRetryAttempt;
    const int delay = m_coverRetryAttempt == 1 ? 250 : 1000;
    QTimer::singleShot(delay, this, [this, mediaKey] {
        m_coverRetryPending = false;
        if (m_session && m_mediaKey == mediaKey)
            updateInfo();
    });
}
void MediaWorker::updateInfo(){
    try{
        auto session=m_session;
        if(!session){
            return;
        }
        auto media=boundedResult(session.TryGetMediaPropertiesAsync());
        QString title=QString::fromWCharArray(media.Title().c_str());
        QString artist=QString::fromWCharArray(media.Artist().c_str());
        QString album=QString::fromWCharArray(media.AlbumTitle().c_str());
        const QString provider = providerName(session);
        const QString mediaKey = provider + QChar(0x1f) + title + QChar(0x1f)
                                 + artist + QChar(0x1f) + album;
        if (mediaKey != m_mediaKey) {
            m_mediaKey = mediaKey;
            m_coverRetryAttempt = 0;
            m_coverRetryPending = false;
        }

        bool needsRetry = false;
        const QImage coverImage = readCover(media, needsRetry);
        if (needsRetry)
            scheduleCoverRetry(mediaKey);
        else
            m_coverRetryAttempt = 0;
        emit mediaInfoUpdated(title,artist,album,provider,coverImage);
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
        const auto info = session.GetPlaybackInfo();
        const auto controls = info.Controls();
        emit controlsUpdated({{"toggle", controls.IsPlayPauseToggleEnabled()},
                              {"next", controls.IsNextEnabled()}, {"previous", controls.IsPreviousEnabled()}});
        bool playing=info.PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
        emit mediaPSUpdated(playing);
    }catch(...){
        qDebug()<<"GMSTC error";
    }
}
void MediaWorker::initialize()
{
    winrt::init_apartment(
        winrt::apartment_type::multi_threaded);

    try { m_manager = boundedResult(SessionManager::RequestAsync()); }
    catch (...) { emit commandStatus(QStringLiteral("无法访问系统媒体会话")); return; }

    m_sessionToken =
        m_manager.CurrentSessionChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    [this]
                    {
                        refreshSessions();
                    },
                    Qt::QueuedConnection);
            });

    m_sessionsToken = m_manager.SessionsChanged([this](auto&&, auto&&) {
        QMetaObject::invokeMethod(this, &MediaWorker::refreshSessions, Qt::QueuedConnection);
    });
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout, this, [this] { refreshSessions(); updatePS(); updateTimeline(); });
    refreshSessions();
    m_pollTimer->start();

    qDebug() << "MediaWorker initialized";
}
void MediaWorker::registerSession()
{
    unregisterSession();
    emit controlsUpdated({});
    m_mediaKey.clear(); m_coverRetryAttempt = 0; m_coverRetryPending = false;
    emit mediaInfoUpdated("", "", "", "", QImage());
    emit mediaPSUpdated(false);
    emit timelineUpdated(0,0,false);
    for (const auto &entry : m_sessions) if (entry.id == m_selectedId) { m_session = entry.session; break; }

    if (!m_session)
    {
        m_mediaKey.clear();
        m_coverRetryAttempt = 0;
        m_coverRetryPending = false;
        emit mediaInfoUpdated(
            "",
            "",
            "",
            "",
            QImage());
        emit mediaPSUpdated(false);
        emit timelineUpdated(0, 0, false);
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
    m_timelineToken =
        m_session.TimelinePropertiesChanged(
            [this](auto&&, auto&&)
            {
                QMetaObject::invokeMethod(
                    this,
                    &MediaWorker::updateTimeline,
                    Qt::QueuedConnection);
            });
    updateTimeline();
}

void MediaWorker::refreshSessions() {
    if (!m_manager) return;
    try {
        std::vector<Entry> next;
        for (const auto &session : m_manager.GetSessions()) {
            auto found = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const Entry &entry) { return entry.session == session; });
            if (found != m_sessions.end()) next.push_back(*found);
            else next.push_back({QStringLiteral("session-%1").arg(m_nextId++), providerName(session), session});
        }
        m_sessions = std::move(next);
        const bool missing = std::none_of(m_sessions.begin(), m_sessions.end(), [&](const Entry &entry) { return entry.id == m_selectedId; });
        if (missing) m_followSystem = true;
        QString chosen = m_selectedId;
        if (m_followSystem) {
            chosen.clear();
            auto current = m_manager.GetCurrentSession();
            for (const auto &entry : m_sessions) if (entry.session == current) { chosen = entry.id; break; }
            if (chosen.isEmpty() && !m_sessions.empty()) chosen = m_sessions.front().id;
        }
        const bool changed = chosen != m_selectedId;
        m_selectedId = chosen;
        QVariantList list;
        for (const auto &entry : m_sessions) list.append(QVariantMap{{"id", entry.id}, {"name", entry.name}, {"selected", entry.id == chosen}});
        emit sessionsUpdated(list, chosen, m_followSystem);
        if (changed || (!m_session && !chosen.isEmpty())) { emit commandStatus({}); registerSession(); }
    } catch (...) { emit commandStatus(QStringLiteral("媒体会话读取失败，将自动重试")); }
}
void MediaWorker::selectSession(const QString &id) {
    refreshSessions();
    auto found = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const Entry &entry) { return entry.id == id; });
    if (found == m_sessions.end()) { emit commandStatus(QStringLiteral("该媒体会话已结束")); return; }
    m_followSystem = false;
    emit commandStatus({});
    const bool changed = m_selectedId != id;
    m_selectedId = id;
    if (changed) registerSession();
    refreshSessions();
}
void MediaWorker::followSystemSession() { m_followSystem = true; refreshSessions(); }
void MediaWorker::updateTimeline() {
    if (!m_session) { emit timelineUpdated(0,0,false); return; }
    try {
        const auto timeline = m_session.GetTimelineProperties();
        const auto info = m_session.GetPlaybackInfo();
        const double start = timeline.StartTime().count()/10000000.0;
        const double duration = std::max(0.0, timeline.EndTime().count()/10000000.0-start);
        double position = timeline.Position().count()/10000000.0-start;
        // Position is a snapshot; use its timestamp and reported rate while playing.
        if (info.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
            const auto updated = timeline.LastUpdatedTime();
            if (updated.time_since_epoch().count() > 0) {
                const double elapsed = std::max(0.0, std::chrono::duration<double>(winrt::clock::now()-updated).count());
                auto rate = info.PlaybackRate();
                position += elapsed * (rate ? rate.Value() : 1.0);
            }
        }
        const bool canSeek = duration > 0 && info.Controls().IsPlaybackPositionEnabled()
            && timeline.MaxSeekTime() > timeline.MinSeekTime();
        emit timelineUpdated(qBound(0.0, position, duration), duration, canSeek);
    } catch (...) { emit timelineUpdated(0,0,false); }
}
void MediaWorker::seek(double seconds, const QString &sessionId) {
    if (!m_session || sessionId != m_selectedId || !std::isfinite(seconds)) return;
    try {
        const auto timeline = m_session.GetTimelineProperties();
        if (!m_session.GetPlaybackInfo().Controls().IsPlaybackPositionEnabled()
            || timeline.MaxSeekTime() <= timeline.MinSeekTime()) return;
        const double requested = timeline.StartTime().count() + std::max(0.0,seconds)*10000000.0;
        const auto ticks = static_cast<int64_t>(qBound(double(timeline.MinSeekTime().count()), requested, double(timeline.MaxSeekTime().count())));
        const bool result = boundedResult(m_session.TryChangePlaybackPositionAsync(ticks));
        emit commandStatus(result ? QString() : QStringLiteral("播放器未接受进度调整"));
        updateTimeline();
    } catch (...) { emit commandStatus(QStringLiteral("进度调整失败，会话可能已结束")); }
}

void MediaWorker::refresh()
{
    refreshSessions();
    updateInfo();
    updatePS();
    updateTimeline();
}
void MediaWorker::unregisterSession()
{
    if (!m_session)
        return;

    try { m_session.MediaPropertiesChanged(m_mediaToken); } catch (...) {}
    try { m_session.PlaybackInfoChanged(m_playbackToken); } catch (...) {}
    try { m_session.TimelinePropertiesChanged(m_timelineToken); } catch (...) {}

    m_session = nullptr;
}
void MediaWorker::togglePlayPause()
{
    if (!m_session)
        return;

    try
    {
        if (!m_session.GetPlaybackInfo().Controls().IsPlayPauseToggleEnabled()) {
            emit commandStatus(QStringLiteral("当前播放器不支持播放/暂停切换")); return;
        }
        const bool result = boundedResult(m_session.TryTogglePlayPauseAsync());
        emit commandStatus(result ? QString() : QStringLiteral("播放器未接受播放/暂停操作"));
    }
    catch (...)
    {
        qDebug() << "Toggle failed";
        emit commandStatus(QStringLiteral("播放/暂停失败，会话可能已结束"));
    }
}
void MediaWorker::invokeControl(const QString &actionId, const QString &sessionId)
{
    if (!m_session || sessionId.isEmpty() || sessionId != m_selectedId) {
        emit commandStatus(QStringLiteral("媒体会话已切换，未执行过期控制")); return;
    }
    if (actionId == QStringLiteral("media.toggle")) togglePlayPause();
    else if (actionId == QStringLiteral("media.next")) next();
    else if (actionId == QStringLiteral("media.previous")) previous();
}
void MediaWorker::next()
{
    if (!m_session)
        return;

    try
    {
        if (!m_session.GetPlaybackInfo().Controls().IsNextEnabled()) {
            emit commandStatus(QStringLiteral("当前播放器不支持下一首")); return;
        }
        const bool result = boundedResult(m_session.TrySkipNextAsync());
        emit commandStatus(result ? QString() : QStringLiteral("播放器未接受下一首操作"));
    }
    catch (...)
    {
        emit commandStatus(QStringLiteral("下一首操作失败"));
    }
}
void MediaWorker::previous()
{
    if (!m_session)
        return;

    try
    {
        if (!m_session.GetPlaybackInfo().Controls().IsPreviousEnabled()) {
            emit commandStatus(QStringLiteral("当前播放器不支持上一首")); return;
        }
        const bool result = boundedResult(m_session.TrySkipPreviousAsync());
        emit commandStatus(result ? QString() : QStringLiteral("播放器未接受上一首操作"));
    }
    catch (...)
    {
        emit commandStatus(QStringLiteral("上一首操作失败"));
    }
}

QString MediaWorker::providerName(const Session &session)
{
    if (!session)
        return {};

    const winrt::hstring sourceId = session.SourceAppUserModelId();
    try {
        const auto appInfo =
            Windows::ApplicationModel::AppInfo::GetFromAppUserModelId(sourceId);
        const QString displayName = QString::fromWCharArray(
            appInfo.DisplayInfo().DisplayName().c_str());
        if (!displayName.isEmpty())
            return displayName;
    } catch (...) {
    }

    QString fallback = QString::fromWCharArray(sourceId.c_str());
    if (fallback.contains('!'))
        fallback = fallback.section('!', -1);
    if (fallback.endsWith(".exe", Qt::CaseInsensitive))
        fallback.chop(4);
    if (fallback.contains('.'))
        fallback = fallback.section('.', -1);
    return fallback.isEmpty() ? QStringLiteral("未知来源") : fallback;
}
