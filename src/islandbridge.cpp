#include "include/islandbridge.h"
#include <QRegularExpression>
#include <cmath>
namespace {
bool validId(const QString &id) { return QRegularExpression("^[A-Za-z0-9._-]{1,64}$").match(id).hasMatch(); }
QVariantMap sanitize(const QString &id,const QVariantMap &state,int priority) {
    QVariantMap safe{{"sourceId",id},{"title",state.value("title").toString().left(128)},
        {"subtitle",state.value("subtitle").toString().left(256)},{"priority",qBound(-100,priority,100)}};
    const auto icon=state.value("iconName").toString();
    if (QRegularExpression("^[a-z0-9_]{1,64}$").match(icon).hasMatch()) safe.insert("iconName",icon);
    const auto route=state.value("route").toString();
    if (validId(route)) safe.insert("route",route);
    // Presentation hints are bounded data, not URLs, markup or plugin code.
    const auto kind=state.value("kind").toString();
    if (QStringList{"input","media","focus","notification","task","awake","ocr","network","usb"}.contains(kind)) safe.insert("kind",kind);
    const auto tone=state.value("tone").toString();
    if (QStringList{"neutral","success","danger"}.contains(tone)) safe.insert("tone",tone);
    safe.insert("detailText",state.value("detailText").toString().left(24));
    bool countOk=false; const int count=state.value("applicationCount").toInt(&countOk);
    if (countOk) safe.insert("applicationCount",qBound(0,count,999));
    bool ok=false; const auto progress=state.value("progress").toDouble(&ok);
    if (ok && std::isfinite(progress)) safe.insert("progress",qBound(0.0,progress,1.0));
    // Typed result metadata arrives only through trusted internal adapters.
    if (state.contains("exitCode")) safe.insert("exitCode",state.value("exitCode"));
    if (state.contains("elapsedMs")) safe.insert("elapsedMs",state.value("elapsedMs"));
    if (state.contains("outcome")) safe.insert("outcome",state.value("outcome").toString().left(24));
    return safe;
}
}
IslandBridge::IslandBridge(QObject *parent, std::function<qint64()> clock) : QObject(parent), m_now(std::move(clock)) {
    m_clock.start(); m_expiryTimer.setInterval(50);
    if (!m_now) m_now = [this] { return m_clock.elapsed(); };
    connect(&m_expiryTimer,&QTimer::timeout,this,&IslandBridge::refresh);
}
bool IslandBridge::publishState(const QString &id,const QVariantMap &state,int priority) { return publish(id,state,priority,0); }
bool IslandBridge::publish(const QString &id,const QVariantMap &state,int priority,int lifetimeMs) {
    if (lifetimeMs>0) return pushEvent(id,"legacy",state,priority,lifetimeMs);
    if (!validId(id) || (m_entries.size()>=32 && !m_entries.contains(id))) return false;
    const auto previous=m_entries.constFind(id);
    const qint64 order=previous==m_entries.cend() ? ++m_order : previous->order;
    m_entries.insert(id,{sanitize(id,state,priority),qBound(-100,priority,100),order,0});
    refresh(); return true;
}
bool IslandBridge::pushEvent(const QString &id,const QString &key,const QVariantMap &state,int priority,int duration,int validFor) {
    if (!validId(id) || !validId(key)) return false;
    const QString token=id+":"+key;
    refresh(); // Drop stale items before enforcing queue capacity.
    const qint64 now=m_now();
    if (m_recent.value(token,0)>now) return false;
    auto found=m_events.find(token);
    if (found!=m_events.end()) {
        // Coalesce without renewing the first event's expiry/display duration.
        found->state=sanitize(id,state,priority); found->priority=qBound(-100,priority,100);
    } else {
        if (m_events.size()>=16) {
            auto lowest=m_events.end();
            for (auto it=m_events.begin(); it!=m_events.end(); ++it) {
                if (it.key()==m_currentEvent) continue;
                if (lowest==m_events.end() || it->priority<lowest->priority
                    || (it->priority==lowest->priority && it->order>lowest->order)) lowest=it;
            }
            if (lowest==m_events.end() || qBound(-100,priority,100)<=lowest->priority) return false;
            m_events.erase(lowest); // An urgent event must not be lost to low-priority flooding.
        }
        m_events.insert(token,{sanitize(id,state,priority),qBound(-100,priority,100),++m_order,
            now+qBound(1000,validFor,60000),qBound(1000,duration,10000)});
    }
    refresh(); return true;
}
void IslandBridge::withdraw(const QString &id) {
    m_entries.remove(id);
    for (auto it=m_events.begin();it!=m_events.end();) {
        if (it->state.value("sourceId")==id) { if (it.key()==m_currentEvent) m_currentEvent.clear(); it=m_events.erase(it); }
        else ++it;
    }
    refresh();
}
void IslandBridge::requestDetails() { refresh(); if (!m_active.isEmpty()) emit detailsRequested(m_active.value("sourceId").toString()); }
void IslandBridge::remember(const QString &token, qint64 now) {
    if (token.isEmpty()) return;
    if (m_recent.size()>=64 && !m_recent.contains(token)) {
        auto oldest=m_recent.begin();
        for (auto it=m_recent.begin(); it!=m_recent.end(); ++it) if (it.value()<oldest.value()) oldest=it;
        m_recent.erase(oldest);
    }
    m_recent.insert(token,now+15000);
}
void IslandBridge::dismissCurrentEvent() {
    if (m_currentEvent.isEmpty()) return;
    const auto token=m_currentEvent;
    m_events.remove(token); m_currentEvent.clear(); remember(token,m_now()); refresh();
}
void IslandBridge::refresh() {
    const qint64 now=m_now();
    for (auto it=m_recent.begin();it!=m_recent.end();) it=it.value()<=now ? m_recent.erase(it) : ++it;
    if (!m_currentEvent.isEmpty() && (now>=m_eventEnd || !m_events.contains(m_currentEvent))) {
        m_events.remove(m_currentEvent);
        remember(m_currentEvent,now); m_currentEvent.clear();
    }
    for (auto it=m_events.begin();it!=m_events.end();) it=it.key()!=m_currentEvent && it->expiry<=now ? m_events.erase(it) : ++it;
    const Entry *persistent=nullptr;
    QString persistentId;
    for (auto it=m_entries.cbegin();it!=m_entries.cend();++it) {
        if (!persistent || it->priority>persistent->priority || (it->priority==persistent->priority && it->order<persistent->order)) { persistent=&it.value(); persistentId=it.key(); }
    }
    QString nextEvent;
    const Entry *event=nullptr;
    for (auto it=m_events.cbegin();it!=m_events.cend();++it) {
        if (!event || it->priority>event->priority || (it->priority==event->priority && it->order<event->order)) { event=&it.value(); nextEvent=it.key(); }
    }
    bool wake=false;
    // Events are buffered, not repeatedly preempted. After a 1.2s minimum only
    // priority 95+ may interrupt, and the interrupted event keeps remaining time.
    if (!m_currentEvent.isEmpty() && nextEvent!=m_currentEvent && event && event->priority>=95 && now-m_eventStart>=1200
        && event->priority>m_events.value(m_currentEvent).priority) {
        m_events[m_currentEvent].duration=int(qMax<qint64>(1,m_eventEnd-now));
        m_currentEvent.clear();
    }
    if (!m_currentEvent.isEmpty() && persistent && persistent->priority>m_events.value(m_currentEvent).priority && now-m_eventStart>=1200) {
        m_events[m_currentEvent].duration=int(qMax<qint64>(1,m_eventEnd-now));
        m_currentEvent.clear();
    }
    if (m_currentEvent.isEmpty() && event && (!persistent || event->priority>=persistent->priority)) {
        m_currentEvent=nextEvent; m_eventStart=now; m_eventEnd=now+event->duration; wake=true;
    }
    QVariantMap active;
    if (!m_currentEvent.isEmpty()) {
        active=m_events.value(m_currentEvent).state; active.insert("mode","event");
        const int duration=m_events.value(m_currentEvent).duration;
        active.insert("eventProgress",qBound(0.0,double(now-m_eventStart)/qMax(1,duration),1.0));
    }
    else if (persistent) {
        active=persistent->state; active.insert("mode","persistent");
        wake=persistentId!=m_stateSource;
    }
    if (m_currentEvent.isEmpty()) m_stateSource=persistentId;
    if (!active.isEmpty()) active.insert("queuedCount",m_events.size());
    if (active!=m_active) { m_active=active; emit activeStateChanged(); }
    if (wake) emit displayRequested();
    if (!m_events.isEmpty() || !m_recent.isEmpty()) { if (!m_expiryTimer.isActive()) m_expiryTimer.start(); }
    else m_expiryTimer.stop();
}
