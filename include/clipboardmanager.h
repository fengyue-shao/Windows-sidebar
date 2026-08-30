#pragma once

#include <QObject>
#include <QVariantList>

class ClipboardManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit ClipboardManager(QObject *parent = nullptr);

    QVariantList items() const;
    QString statusText() const;
    bool loading() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void activateItem(const QString &itemId);
    Q_INVOKABLE void togglePinned(const QString &itemId);
    Q_INVOKABLE void deleteItem(const QString &itemId);
    Q_INVOKABLE void clearHistory();

    void applyHistory(const QVariantList &items, const QString &statusText);
    void applyDeleteResult(const QString &itemId, bool deleted, const QString &statusText);
    void applyClearResult(int deletedCount, const QString &statusText);
    QStringList pinnedIds() const;

signals:
    void itemsChanged();
    void statusChanged();
    void loadingChanged();

private:
    QVariantList m_items;
    QString m_statusText = QStringLiteral("打开页面后读取 Windows 剪贴板历史");
    bool m_loading = false;
    QStringList m_pinnedIds;
};
