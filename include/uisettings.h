#pragma once

#include <QObject>

class UiSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(int currentSection READ currentSection WRITE setCurrentSection NOTIFY currentSectionChanged)
    Q_PROPERTY(bool mediaEnabled READ mediaEnabled WRITE setMediaEnabled NOTIFY mediaEnabledChanged)
    Q_PROPERTY(bool monitorEnabled READ monitorEnabled WRITE setMonitorEnabled NOTIFY monitorEnabledChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)

public:
    explicit UiSettings(QObject *parent = nullptr);

    bool expanded() const;
    int currentSection() const;
    bool mediaEnabled() const;
    bool monitorEnabled() const;
    bool notificationsEnabled() const;

public slots:
    void setExpanded(bool expanded);
    void setCurrentSection(int section);
    void setMediaEnabled(bool enabled);
    void setMonitorEnabled(bool enabled);
    void setNotificationsEnabled(bool enabled);
    Q_INVOKABLE void reset();

signals:
    void expandedChanged();
    void currentSectionChanged();
    void mediaEnabledChanged();
    void monitorEnabledChanged();
    void notificationsEnabledChanged();

private:
    bool m_expanded = true;
    int m_currentSection = 0;
    bool m_mediaEnabled = true;
    bool m_monitorEnabled = true;
    bool m_notificationsEnabled = true;
};
