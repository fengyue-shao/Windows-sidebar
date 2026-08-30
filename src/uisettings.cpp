#include "include/uisettings.h"

#include <QSettings>

namespace {
constexpr auto expandedKey = "layout/expanded";
constexpr auto currentSectionKey = "layout/currentSection";
constexpr auto mediaEnabledKey = "modules/mediaEnabled";
constexpr auto monitorEnabledKey = "modules/monitorEnabled";
constexpr auto notificationsEnabledKey = "modules/notificationsEnabled";
}

UiSettings::UiSettings(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_expanded = settings.value(expandedKey, true).toBool();
    m_currentSection = settings.value(currentSectionKey, 0).toInt();
    if (m_currentSection < 0 || m_currentSection > 2)
        m_currentSection = 0;
    m_mediaEnabled = settings.value(mediaEnabledKey, true).toBool();
    m_monitorEnabled = settings.value(monitorEnabledKey, true).toBool();
    m_notificationsEnabled = settings.value(notificationsEnabledKey, true).toBool();
}

bool UiSettings::expanded() const { return m_expanded; }
int UiSettings::currentSection() const { return m_currentSection; }
bool UiSettings::mediaEnabled() const { return m_mediaEnabled; }
bool UiSettings::monitorEnabled() const { return m_monitorEnabled; }
bool UiSettings::notificationsEnabled() const { return m_notificationsEnabled; }

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
    if (section < 0 || section > 2 || m_currentSection == section)
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

void UiSettings::reset()
{
    setExpanded(true);
    setCurrentSection(0);
    setMediaEnabled(true);
    setMonitorEnabled(true);
    setNotificationsEnabled(true);
}
