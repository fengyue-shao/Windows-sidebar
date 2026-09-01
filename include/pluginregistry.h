#pragma once

#include <QObject>
#include <QVariantList>

class PluginRegistry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pagesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit PluginRegistry(QObject *parent = nullptr);

    QVariantList pages() const { return m_pages; }
    int pageCount() const { return m_pages.size(); }
    QString statusText() const { return m_statusText; }

    Q_INVOKABLE void reload();

signals:
    void pagesChanged();
    void statusChanged();

private:
    QVariantList m_pages;
    QString m_statusText;
};
