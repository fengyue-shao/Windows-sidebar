#pragma once

#include <QObject>
#include <QVariantList>
#include <QHash>
#include <QPointer>
#include <QSet>
class PluginProcess;
class IslandBridge;

class PluginRegistry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pagesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString errorDetails READ errorDetails NOTIFY statusChanged)

public:
    explicit PluginRegistry(QObject *parent = nullptr, const QString &pluginRoot = {});
    ~PluginRegistry() override;
    void setIslandBridge(IslandBridge *bridge);
    void setThemeTokens(const QVariantMap &tokens);
    static QString packageError(const QString &path);

    QVariantList pages() const { return m_pages; }
    QVariantList plugins() const { return m_plugins; }
    int pageCount() const { return m_pages.size(); }
    QString statusText() const { return m_statusText; }
    QString errorDetails() const { return m_errorDetails; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setPluginEnabled(const QString &pluginId, bool enabled);
    Q_INVOKABLE void launch(const QString &pluginId);
    Q_INVOKABLE void stop(const QString &pluginId);
    Q_INVOKABLE bool openRoute(const QString &route);
    Q_INVOKABLE void trustAndEnable(const QString &pluginId);
    Q_INVOKABLE bool selectVersion(const QString &pluginId, const QString &version, bool confirmStop = false);
    Q_INVOKABLE bool importTrustedFolder(const QString &path);
    Q_INVOKABLE void openPluginFolder();

signals:
    void pagesChanged();
    void pluginsChanged();
    void statusChanged();

private:
    QVariantList m_pages;
    QVariantList m_plugins;
    QString m_statusText;
    QString m_errorDetails;
    QHash<QString, PluginProcess *> m_processes;
    QHash<QString, QString> m_executables, m_routes;
    QHash<QString, QString> m_runningExecutables, m_pendingVersions;
    QHash<QString, QSet<QString>> m_pageIds;
    QPointer<IslandBridge> m_bridge;
    QString m_rootPath;
    QVariantMap m_themeTokens;
    void updateProcessStatus();
    void finishVersionSelection(const QString &pluginId, const QString &version);
};
