#include "include/dropdowncontroller.h"
#include "include/fullscreenpolicy.h"
#include "include/hotkeypolicy.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QPainterPath>
#include <QVariantMap>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

DropdownController::DropdownController(QObject *parent) : QObject(parent)
{
    qGuiApp->installNativeEventFilter(this);
    // Thread-bound registration still receives WM_HOTKEY while the window is hidden.
    m_hotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT; m_hotkeyKey = VK_SPACE;
    m_hotkey = RegisterHotKey(nullptr, m_hotkeyId, m_hotkeyModifiers, m_hotkeyKey);
    if (!m_hotkey) qWarning() << "Dropdown hotkey unavailable, Win32 error:" << GetLastError();
    m_timer.setInterval(60);
    connect(&m_timer, &QTimer::timeout, this, &DropdownController::pollPointer);
    m_timer.start();
}

DropdownController::~DropdownController()
{
    if (m_hotkey) UnregisterHotKey(nullptr, m_hotkeyId);
    qGuiApp->removeNativeEventFilter(this);
}

void DropdownController::attach(QWindow *window) { m_window = window; }

bool DropdownController::applyShortcut(const QString &shortcut)
{
    const auto parsed = SidebarHotkey::parse(shortcut);
    if (!parsed.valid) {
        m_hotkeyStatus = QStringLiteral("快捷键无效：使用 Ctrl/Alt/Win 与字母、数字、F1–F24 或 Space 的单一组合");
        emit hotkeyAvailableChanged(); return false;
    }
    if (m_hotkey && parsed.modifiers == m_hotkeyModifiers && parsed.key == m_hotkeyKey) {
        m_hotkeyStatus = QStringLiteral("快捷键已生效"); emit hotkeyAvailableChanged(); return true;
    }
    const int candidate = m_hotkeyId == 0x5342 ? 0x5343 : 0x5342;
    const auto result = SidebarHotkey::replace(m_hotkey,
        [&] { return RegisterHotKey(nullptr, candidate, parsed.modifiers, parsed.key) ? DWORD{0} : GetLastError(); },
        [&] { return UnregisterHotKey(nullptr, m_hotkeyId) ? DWORD{0} : GetLastError(); },
        [&] { UnregisterHotKey(nullptr, candidate); });
    if (!result.accepted) {
        m_hotkeyStatus = QStringLiteral("快捷键注册失败（错误 %1）；保留原注册，不保存新值").arg(result.error);
        emit hotkeyAvailableChanged(); return false;
    }
    m_hotkey = true; m_hotkeyId = candidate; m_hotkeyModifiers = parsed.modifiers; m_hotkeyKey = parsed.key; m_shortcut = parsed.text;
    m_hotkeyStatus = QStringLiteral("快捷键已生效"); emit hotkeyAvailableChanged(); return true;
}

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
    if (m_suppressFullscreen && foregroundCoversTargetMonitor()) { m_dwell = 0; return; }
    if (++m_dwell >= 4) { m_blocked = true; m_dwell = 0; emit edgeRequested(); }
}

bool DropdownController::foregroundCoversTargetMonitor() const
{
    if (!m_window) return false;
    const HWND foreground = GetForegroundWindow();
    const HWND own = reinterpret_cast<HWND>(m_window->winId());
    if (!foreground || foreground == own || foreground == GetDesktopWindow() || foreground == GetShellWindow()
        || !IsWindowVisible(foreground) || IsIconic(foreground)) return false;
    wchar_t className[128]{}; GetClassNameW(foreground, className, 128);
    if (wcscmp(className, L"WorkerW") == 0 || wcscmp(className, L"Progman") == 0 || wcscmp(className, L"Shell_TrayWnd") == 0) return false;
    const auto monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    if (!monitor || monitor != MonitorFromWindow(own, MONITOR_DEFAULTTONEAREST)) return false;
    RECT bounds{}; MONITORINFO info{}; info.cbSize = sizeof(info);
    if (!GetWindowRect(foreground, &bounds) || !GetMonitorInfoW(monitor, &info)) return false;
    return SidebarFullscreen::coversMonitor(QRect(bounds.left, bounds.top, bounds.right-bounds.left, bounds.bottom-bounds.top),
        QRect(info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right-info.rcMonitor.left, info.rcMonitor.bottom-info.rcMonitor.top));
}

bool DropdownController::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY || msg->wParam != m_hotkeyId || !m_hotkey) return false;
    emit hotkeyRequested();
    if (result) *result = 0;
    return true;
}
