#include "include/islandbridge.h"
#include <QRegularExpression>
#include <cmath>

IslandBridge::IslandBridge(QObject *parent) : QObject(parent) {
    m_clock.start(); m_expiryTimer.setInterval(250);
    connect(&m_expiryTimer, &QTimer::timeout, this, &IslandBridge::refresh);
}
bool IslandBridge::publish(const QString &sourceId, const QVariantMap &state, int priority, int lifetimeMs) {
    if (!QRegularExpression("^[A-Za-z0-9._-]{1,64}$").match(sourceId).hasMatch()
        || (m_entries.size() >= 32 && !m_entries.contains(sourceId))) return false;
    QVariantMap safe{{"sourceId", sourceId}, {"title", state.value("title").toString().left(128)},
                     {"subtitle", state.value("subtitle").toString().left(256)}};
    const QString icon = state.value("iconName").toString();
    if (QRegularExpression("^[a-z0-9_]{1,64}$").match(icon).hasMatch()) safe.insert("iconName",icon);
    bool ok = false;
    const double progress = state.value("progress").toDouble(&ok);
    if (ok && std::isfinite(progress)) safe.insert("progress",qBound(0.0,progress,1.0));
    const int boundedPriority = qBound(-100,priority,100);
    safe.insert("priority",boundedPriority);
    m_entries.insert(sourceId,{safe,boundedPriority,++m_order,lifetimeMs > 0 ? m_clock.elapsed()+qBound(250,lifetimeMs,60000) : 0});
    refresh(); return true;
}
void IslandBridge::withdraw(const QString &sourceId) { m_entries.remove(sourceId); refresh(); }
void IslandBridge::requestDetails() {
    refresh();
    if (!m_active.isEmpty()) emit detailsRequested(m_active.value("sourceId").toString());
}
void IslandBridge::refresh() {
    const qint64 now = m_clock.elapsed();
    const Entry *best = nullptr;
    bool expiring = false;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it->expiry > 0 && it->expiry <= now) it = m_entries.erase(it);
        else ++it;
    }
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        expiring |= it->expiry > 0;
        if (!best || it->priority > best->priority || (it->priority == best->priority && it->order > best->order)) best = &it.value();
    }
    const QVariantMap active = best ? best->state : QVariantMap{};
    if (active != m_active) { m_active = active; emit activeStateChanged(); }
    if (expiring) m_expiryTimer.start(); else m_expiryTimer.stop();
}
