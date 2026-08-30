#include "include/displaymanager.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QVariantMap>

DisplayManager::DisplayManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_selectedName = settings.value(QStringLiteral("appearance/display")).toString();
    const int storedDockSide = settings.value(QStringLiteral("appearance/dockSide"), Right).toInt();
    if (storedDockSide == Left || storedDockSide == Right)
        m_dockSide = static_cast<DockSide>(storedDockSide);

    connect(qGuiApp, &QGuiApplication::screenAdded,
            this, &DisplayManager::rebuildScreens);
    connect(qGuiApp, &QGuiApplication::screenRemoved,
            this, &DisplayManager::rebuildScreens);
    rebuildScreens();
}

QVariantList DisplayManager::displays() const
{
    return m_displays;
}

int DisplayManager::currentIndex() const
{
    return m_currentIndex;
}

QRect DisplayManager::selectedGeometry() const
{
    return m_selectedGeometry;
}

int DisplayManager::selectedX() const { return m_selectedGeometry.x(); }
int DisplayManager::selectedY() const { return m_selectedGeometry.y(); }
int DisplayManager::selectedWidth() const { return m_selectedGeometry.width(); }
int DisplayManager::selectedHeight() const { return m_selectedGeometry.height(); }
int DisplayManager::dockSide() const { return static_cast<int>(m_dockSide); }

void DisplayManager::setDockSide(int side)
{
    if (side != Left && side != Right)
        return;
    const DockSide newSide = static_cast<DockSide>(side);
    if (newSide == m_dockSide)
        return;
    m_dockSide = newSide;
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/dockSide"), side);
    settings.sync();
    emit dockSideChanged();
}

void DisplayManager::setCurrentIndex(int index)
{
    const QList<QScreen *> screens = qGuiApp->screens();
    if (index < 0 || index >= screens.size())
        return;

    QScreen *screen = screens.at(index);
    const bool indexChanged = index != m_currentIndex;
    const bool geometryChanged = screen->geometry() != m_selectedGeometry;
    m_currentIndex = index;
    m_selectedName = screen->name();
    m_selectedGeometry = screen->geometry();

    QSettings settings;
    settings.setValue(QStringLiteral("appearance/display"), m_selectedName);

    if (indexChanged)
        emit currentIndexChanged();
    if (geometryChanged || indexChanged)
        emit selectedGeometryChanged();
}

void DisplayManager::rebuildScreens()
{
    const QList<QScreen *> screens = qGuiApp->screens();
    if (screens.isEmpty())
        return;

    for (QScreen *screen : screens) {
        connect(screen, &QScreen::geometryChanged,
                this, &DisplayManager::refreshSelectedGeometry,
                Qt::UniqueConnection);
    }

    int selectedIndex = -1;
    for (int index = 0; index < screens.size(); ++index) {
        if (screens.at(index)->name() == m_selectedName) {
            selectedIndex = index;
            break;
        }
    }
    if (selectedIndex < 0)
        selectedIndex = screens.indexOf(qGuiApp->primaryScreen());
    if (selectedIndex < 0)
        selectedIndex = 0;

    QVariantList newDisplays;
    for (int index = 0; index < screens.size(); ++index) {
        QScreen *screen = screens.at(index);
        QString label = screen->name();
        if (!screen->model().isEmpty() && screen->model() != label)
            label += QStringLiteral(" · ") + screen->model();
        if (screen == qGuiApp->primaryScreen())
            label += QStringLiteral("（主显示器）");

        QVariantMap display;
        display.insert(QStringLiteral("index"), index);
        display.insert(QStringLiteral("name"), screen->name());
        display.insert(QStringLiteral("label"), label);
        newDisplays.append(display);
    }

    const bool displaysChanged = newDisplays != m_displays;
    const bool indexChanged = selectedIndex != m_currentIndex;
    const QRect geometry = screens.at(selectedIndex)->geometry();
    const bool geometryChanged = geometry != m_selectedGeometry;

    m_displays = newDisplays;
    m_currentIndex = selectedIndex;
    m_selectedName = screens.at(selectedIndex)->name();
    m_selectedGeometry = geometry;

    if (displaysChanged)
        emit this->displaysChanged();
    if (indexChanged)
        emit currentIndexChanged();
    if (geometryChanged || indexChanged)
        emit selectedGeometryChanged();
}

void DisplayManager::refreshSelectedGeometry()
{
    const QList<QScreen *> screens = qGuiApp->screens();
    if (m_currentIndex < 0 || m_currentIndex >= screens.size()) {
        rebuildScreens();
        return;
    }

    const QRect geometry = screens.at(m_currentIndex)->geometry();
    if (geometry == m_selectedGeometry)
        return;

    m_selectedGeometry = geometry;
    emit selectedGeometryChanged();
}
