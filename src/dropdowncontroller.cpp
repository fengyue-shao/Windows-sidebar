#include "include/dropdowncontroller.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QPainterPath>
#include <QVariantMap>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace { constexpr int hotkeyId = 0x5342; }

DropdownController::DropdownController(QObject *parent) : QObject(parent)
{
    qGuiApp->installNativeEventFilter(this);
    // Thread-bound registration still receives WM_HOTKEY while the window is hidden.
    m_hotkey = RegisterHotKey(nullptr, hotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_SPACE);
    if (!m_hotkey) qWarning() << "Dropdown hotkey unavailable, Win32 error:" << GetLastError();
    m_timer.setInterval(60);
    connect(&m_timer, &QTimer::timeout, this, &DropdownController::pollPointer);
    m_timer.start();
}

DropdownController::~DropdownController()
{
    if (m_hotkey) UnregisterHotKey(nullptr, hotkeyId);
    qGuiApp->removeNativeEventFilter(this);
}

void DropdownController::attach(QWindow *window) { m_window = window; }

void DropdownController::configureEdge(int x, int y, int width)
{
    const QRect edge(x, y, qMax(0, width), 3);
    if (edge == m_edge) return;
    m_edge = edge;
    m_dwell = 0;
}

void DropdownController::blockEdgeUntilLeave() { m_blocked = true; m_dwell = 0; }

void DropdownController::setRegions(const QVariantList &rectangles)
{
    if (!m_window) return;
    QRegion region;
    const QRect bounds(QPoint(0, 0), m_window->size());
    for (const QVariant &entry : rectangles) {
        const QVariantMap r = entry.toMap();
        const qreal x = r.value("x").toDouble(), y = r.value("y").toDouble();
        const qreal width = r.value("width").toDouble(), height = r.value("height").toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)
            || width <= 0 || height <= 0) continue;
        QRect rect = QRectF(x, y, width, height).toAlignedRect().intersected(bounds);
        if (rect.isEmpty()) continue;
        const qreal radius = qBound(0.0, r.value("radius").toDouble(), qMin(width, height) / 2.0);
        // Preserve rounded corners. Clipped scrolling cards use a square intersection.
        if (radius > 0 && bounds.contains(QRectF(x, y, width, height).toAlignedRect())) {
            QPainterPath path;
            path.addRoundedRect(QRectF(rect), radius, radius);
            region |= QRegion(path.toFillPolygon().toPolygon());
        } else region |= rect;
    }
    // An empty QWindow mask means unrestricted input, not an empty region.
    if (region.isEmpty()) region = QRegion(QRect(0, 0, 1, 1));
    if (region == m_region) return;
    m_region = region;
    m_window->setMask(region);
}

void DropdownController::pollPointer()
{
    const QPoint cursor = QCursor::pos();
    const bool inside = m_window && m_window->isVisible()
        && (m_region.contains(m_window->mapFromGlobal(cursor)) || m_edge.contains(cursor));
    if (inside != m_inside) { m_inside = inside; emit pointerInsideChanged(); }
    if (!m_edge.contains(cursor)) { m_blocked = false; m_dwell = 0; return; }
    if (m_blocked || (m_window && m_window->isVisible())) return;
    if (++m_dwell >= 4) { m_blocked = true; m_dwell = 0; emit edgeRequested(); }
}

bool DropdownController::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY || msg->wParam != hotkeyId) return false;
    emit hotkeyRequested();
    if (result) *result = 0;
    return true;
}
