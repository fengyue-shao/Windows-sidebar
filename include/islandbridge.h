#pragma once
#include <QObject>
#include <QVariantMap>
#include <QHash>
#include <QElapsedTimer>
#include <QTimer>

// Data-only extension point; it does not execute plugin code or grant authority.
class IslandBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap activeState READ activeState NOTIFY activeStateChanged)
public:
    explicit IslandBridge(QObject *parent = nullptr);
    QVariantMap activeState() const { return m_active; }
    Q_INVOKABLE bool publish(const QString &sourceId, const QVariantMap &state, int priority = 0, int lifetimeMs = 0);
    Q_INVOKABLE void withdraw(const QString &sourceId);
    Q_INVOKABLE void requestDetails();
signals:
    void activeStateChanged();
    void detailsRequested(const QString &sourceId);
private:
    struct Entry { QVariantMap state; int priority; qint64 order, expiry; };
    QHash<QString, Entry> m_entries;
    QVariantMap m_active;
    QElapsedTimer m_clock;
    QTimer m_expiryTimer;
    qint64 m_order = 0;
    void refresh();
};
