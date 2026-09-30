#include "include/uisettings.h"

#include <QSettings>

namespace {
constexpr auto expandedKey = "layout/expanded";
constexpr auto currentSectionKey = "layout/currentSection";
constexpr auto mediaEnabledKey = "modules/mediaEnabled";
constexpr auto monitorEnabledKey = "modules/monitorEnabled";
constexpr auto notificationsEnabledKey = "modules/notificationsEnabled";
constexpr auto autoHideKey = "behavior/autoHide";
constexpr auto compactModeKey = "appearance/compactMode";
constexpr auto panelWidthKey = "appearance/panelWidth";
constexpr auto panelOpacityKey = "appearance/panelOpacity";
constexpr auto navigationSchemaKey = "layout/navigationSchema";
}

UiSettings::UiSettings(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_expanded = settings.value(expandedKey, true).toBool();
    m_currentSection = settings.value(currentSectionKey, 0).toInt();
    if (settings.value(navigationSchemaKey, 1).toInt() < 2) {
        m_currentSection = m_currentSection > 0 ? m_currentSection - 1 : 0;
        settings.setValue(currentSectionKey, m_currentSection);
        settings.setValue(navigationSchemaKey, 2);
    }
    if (settings.value(navigationSchemaKey, 2).toInt() < 3) {
        // The former dynamic toolbox was after the terminal; it is now a permanent page.
        if (m_currentSection == 4) m_currentSection = 5;
        else if (m_currentSection == 5) m_currentSection = 4;
        settings.setValue(currentSectionKey, m_currentSection);
        settings.setValue(navigationSchemaKey, 3);
    }
    if (m_currentSection < 0 || m_currentSection > 63)
        m_currentSection = 0;
    m_mediaEnabled = settings.value(mediaEnabledKey, true).toBool();
    m_monitorEnabled = settings.value(monitorEnabledKey, true).toBool();
    m_notificationsEnabled = settings.value(notificationsEnabledKey, true).toBool();
    m_autoHide = settings.value(autoHideKey, false).toBool();
    m_compactMode = settings.value(compactModeKey, false).toBool();
    m_panelWidth = qBound(360, settings.value(panelWidthKey, 420).toInt(), 560);
    // Standalone cards no longer have the old sidebar background underneath.
    m_panelOpacity = qBound(0.94, settings.value(panelOpacityKey, 1.0).toDouble(), 1.0);
}

bool UiSettings::expanded() const { return m_expanded; }
int UiSettings::currentSection() const { return m_currentSection; }
bool UiSettings::mediaEnabled() const { return m_mediaEnabled; }
bool UiSettings::monitorEnabled() const { return m_monitorEnabled; }
bool UiSettings::notificationsEnabled() const { return m_notificationsEnabled; }
bool UiSettings::autoHide() const { return m_autoHide; }
bool UiSettings::compactMode() const { return m_compactMode; }
int UiSettings::panelWidth() const { return m_panelWidth; }
qreal UiSettings::panelOpacity() const { return m_panelOpacity; }

void UiSettings::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    QSettings().setValue(expandedKey, expanded);
    emit expandedChanged();
}

void UiSettings::setCurrentSection(int section)
{
    if (section < 0 || section > 63 || m_currentSection == section)
        return;
    m_currentSection = section;
    QSettings().setValue(currentSectionKey, section);
    emit currentSectionChanged();
}

void UiSettings::setMediaEnabled(bool enabled)
{
    if (m_mediaEnabled == enabled)
        return;
    m_mediaEnabled = enabled;
    QSettings().setValue(mediaEnabledKey, enabled);
    emit mediaEnabledChanged();
}

void UiSettings::setMonitorEnabled(bool enabled)
{
    if (m_monitorEnabled == enabled)
        return;
    m_monitorEnabled = enabled;
    QSettings().setValue(monitorEnabledKey, enabled);
    emit monitorEnabledChanged();
}

void UiSettings::setNotificationsEnabled(bool enabled)
{
    if (m_notificationsEnabled == enabled)
        return;
    m_notificationsEnabled = enabled;
    QSettings().setValue(notificationsEnabledKey, enabled);
    emit notificationsEnabledChanged();
}

void UiSettings::setAutoHide(bool enabled)
{
    if (m_autoHide == enabled)
        return;
    m_autoHide = enabled;
    QSettings().setValue(autoHideKey, enabled);
    emit autoHideChanged();
}

void UiSettings::setCompactMode(bool enabled)
{
    if (m_compactMode == enabled)
        return;
    m_compactMode = enabled;
    QSettings().setValue(compactModeKey, enabled);
    emit compactModeChanged();
}

void UiSettings::setPanelWidth(int width)
{
    width = qBound(360, width, 560);
    if (m_panelWidth == width)
        return;
    m_panelWidth = width;
    QSettings().setValue(panelWidthKey, width);
    emit panelWidthChanged();
}

void UiSettings::setPanelOpacity(qreal opacity)
{
    opacity = qBound(0.94, opacity, 1.0);
    if (qFuzzyCompare(m_panelOpacity, opacity))
        return;
    m_panelOpacity = opacity;
    QSettings().setValue(panelOpacityKey, opacity);
    emit panelOpacityChanged();
}

void UiSettings::reset()
{
    setExpanded(true);
    setCurrentSection(0);
    setMediaEnabled(true);
    setMonitorEnabled(true);
    setNotificationsEnabled(true);
    setAutoHide(false);
    setCompactMode(false);
    setPanelWidth(420);
    setPanelOpacity(1.0);
}
