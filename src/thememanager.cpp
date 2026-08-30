#include "include/thememanager.h"

#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QTimer>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    const int storedMode = settings.value("appearance/theme", FollowSystem).toInt();
    if (storedMode >= FollowSystem && storedMode <= Dark)
        m_mode = static_cast<Mode>(storedMode);

    connect(qGuiApp, &QGuiApplication::paletteChanged, this,
            [this] {
                if (m_mode == FollowSystem)
                    emit themeChanged();
            });
    applyColorScheme();
}

ThemeManager::Mode ThemeManager::mode() const { return m_mode; }

bool ThemeManager::dark() const
{
    if (m_mode == Dark)
        return true;
    if (m_mode == Light)
        return false;
    return systemDark();
}

QColor ThemeManager::background() const { return choose("#F2F4F8", "#F21A1B20"); }
QColor ThemeManager::surface() const { return choose("#E9FFFFFF", "#12FFFFFF"); }
QColor ThemeManager::surfaceStrong() const { return choose("#F7FFFFFF", "#E923252C"); }
QColor ThemeManager::surfaceHover() const { return choose("#FFFFFFFF", "#20FFFFFF"); }
QColor ThemeManager::border() const { return choose("#1F253044", "#18FFFFFF"); }
QColor ThemeManager::borderStrong() const { return choose("#33253044", "#30FFFFFF"); }
QColor ThemeManager::textPrimary() const { return choose("#E81C2430", "#F2FFFFFF"); }
QColor ThemeManager::textSecondary() const { return choose("#B82B3545", "#B8FFFFFF"); }
QColor ThemeManager::textMuted() const { return choose("#8A354154", "#80FFFFFF"); }
QColor ThemeManager::textFaint() const { return choose("#623B4658", "#50FFFFFF"); }
QColor ThemeManager::accent() const { return choose("#347FD1", "#61A9FF"); }
QColor ThemeManager::accentSecondary() const { return choose("#7464D9", "#9B8CFF"); }
QColor ThemeManager::accentHover() const { return choose("#4691E5", "#79B9FF"); }
QColor ThemeManager::accentSoft() const { return choose("#20347FD1", "#2861A9FF"); }
QColor ThemeManager::success() const { return choose("#25855E", "#62C4A4"); }
QColor ThemeManager::danger() const { return choose("#C94747", "#FF7777"); }
QColor ThemeManager::dangerSoft() const { return choose("#20C94747", "#30FF6565"); }
QColor ThemeManager::warning() const { return choose("#C4772E", "#F19A68"); }
QColor ThemeManager::track() const { return choose("#18253044", "#20FFFFFF"); }
QColor ThemeManager::mediaStart() const { return choose("#D8E6F5", "#302D4264"); }
QColor ThemeManager::mediaMiddle() const { return choose("#EDF2F8", "#E8222732"); }
QColor ThemeManager::mediaEnd() const { return choose("#F7F9FC", "#F21B1D23"); }
QColor ThemeManager::mediaCover() const { return choose("#DCE3EB", "#30202730"); }
QColor ThemeManager::overlayStart() const { return QColor("#00000000"); }
QColor ThemeManager::overlayEnd() const { return QColor("#B0000000"); }
QColor ThemeManager::overlayText() const { return QColor("#F2FFFFFF"); }

void ThemeManager::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    QSettings settings;
    settings.setValue("appearance/theme", static_cast<int>(mode));
    settings.sync();

    // Notify QML immediately from the application-owned mode, then notify once
    // more after Qt has propagated the platform palette change.
    emit themeChanged();
    applyColorScheme();
    QTimer::singleShot(0, this, &ThemeManager::themeChanged);
}

void ThemeManager::setModeValue(int mode)
{
    if (mode < FollowSystem || mode > Dark)
        return;
    setMode(static_cast<Mode>(mode));
}

void ThemeManager::applyColorScheme()
{
    if (m_mode == Light)
        qGuiApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
    else if (m_mode == Dark)
        qGuiApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    else
        qGuiApp->styleHints()->unsetColorScheme();
}

bool ThemeManager::systemDark() const
{
    return qGuiApp->palette().color(QPalette::Window).lightness() < 128;
}

QColor ThemeManager::choose(const char *lightColor, const char *darkColor) const
{
    return QColor(dark() ? darkColor : lightColor);
}
