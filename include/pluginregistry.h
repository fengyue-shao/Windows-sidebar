#pragma once

#include <QObject>
#include <QVariantList>

class PluginRegistry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pagesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit PluginRegistry(QObject *parent = nullptr);

    QVariantList pages() const { return m_pages; }
    QVariantList plugins() const { return m_plugins; }
    int pageCount() const { return m_pages.size(); }
    QString statusText() const { return m_statusText; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setPluginEnabled(const QString &pluginId, bool enabled);

signals:
    void pagesChanged();
    void pluginsChanged();
    void statusChanged();

private:
    QVariantList m_pages;
    QVariantList m_plugins;
    QString m_statusText;
};
