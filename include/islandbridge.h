#pragma once
#include <QObject>
#include <QVariantMap>
#include <QHash>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>

// Data-only extension point; it does not execute plugin code or grant authority.
class IslandBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap activeState READ activeState NOTIFY activeStateChanged)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY activeStateChanged)
    Q_PROPERTY(bool eventActive READ eventActive NOTIFY activeStateChanged)
public:
    explicit IslandBridge(QObject *parent = nullptr, std::function<qint64()> clock = {});
    QVariantMap activeState() const { return m_active; }
    int queuedCount() const { return m_events.size(); }
    bool eventActive() const { return !m_currentEvent.isEmpty(); }
    Q_INVOKABLE bool publishState(const QString &sourceId, const QVariantMap &state, int priority);
    Q_INVOKABLE bool pushEvent(const QString &sourceId, const QString &key, const QVariantMap &state, int priority, int durationMs = 4000, int validForMs = 15000);
    Q_INVOKABLE bool publish(const QString &sourceId, const QVariantMap &state, int priority = 0, int lifetimeMs = 0);
    Q_INVOKABLE void withdraw(const QString &sourceId);
    Q_INVOKABLE void requestDetails();
    Q_INVOKABLE void dismissCurrentEvent();
signals:
    void activeStateChanged();
    void detailsRequested(const QString &sourceId);
    void displayRequested();
private:
    struct Entry { QVariantMap state; int priority; qint64 order, expiry; int duration = 0; };
    QHash<QString, Entry> m_entries;
    QHash<QString, Entry> m_events;
    QHash<QString, qint64> m_recent;
    QString m_currentEvent;
    QString m_stateSource;
    qint64 m_eventStart = 0, m_eventEnd = 0;
    QVariantMap m_active;
    QElapsedTimer m_clock;
    QTimer m_expiryTimer;
    qint64 m_order = 0;
    std::function<qint64()> m_now;
    void remember(const QString &token, qint64 now);
private slots:
    void refresh();
};
