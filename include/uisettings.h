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
    Q_PROPERTY(bool autoHide READ autoHide WRITE setAutoHide NOTIFY autoHideChanged)
    Q_PROPERTY(bool compactMode READ compactMode WRITE setCompactMode NOTIFY compactModeChanged)
    Q_PROPERTY(int panelWidth READ panelWidth WRITE setPanelWidth NOTIFY panelWidthChanged)
    Q_PROPERTY(qreal panelOpacity READ panelOpacity WRITE setPanelOpacity NOTIFY panelOpacityChanged)

public:
    explicit UiSettings(QObject *parent = nullptr);

    bool expanded() const;
    int currentSection() const;
    bool mediaEnabled() const;
    bool monitorEnabled() const;
    bool notificationsEnabled() const;
    bool autoHide() const;
    bool compactMode() const;
    int panelWidth() const;
    qreal panelOpacity() const;

public slots:
    void setExpanded(bool expanded);
    void setCurrentSection(int section);
    void setMediaEnabled(bool enabled);
    void setMonitorEnabled(bool enabled);
    void setNotificationsEnabled(bool enabled);
    void setAutoHide(bool enabled);
    void setCompactMode(bool enabled);
    void setPanelWidth(int width);
    void setPanelOpacity(qreal opacity);
    Q_INVOKABLE void reset();

signals:
    void expandedChanged();
    void currentSectionChanged();
    void mediaEnabledChanged();
    void monitorEnabledChanged();
    void notificationsEnabledChanged();
    void autoHideChanged();
    void compactModeChanged();
    void panelWidthChanged();
    void panelOpacityChanged();

private:
    bool m_expanded = true;
    int m_currentSection = 0;
    bool m_mediaEnabled = true;
    bool m_monitorEnabled = true;
    bool m_notificationsEnabled = true;
    bool m_autoHide = false;
    bool m_compactMode = false;
    int m_panelWidth = 420;
    qreal m_panelOpacity = 1.0;
};
