#pragma once
#include <QRect>
#include <QtGlobal>
namespace SidebarFullscreen {
// A conservative geometry heuristic, not a claim of exclusive fullscreen.
inline bool coversMonitor(const QRect &window, const QRect &monitor) {
    return !window.isEmpty() && !monitor.isEmpty()
        && qAbs(window.left() - monitor.left()) <= 2 && qAbs(window.top() - monitor.top()) <= 2
        && qAbs(window.right() - monitor.right()) <= 2 && qAbs(window.bottom() - monitor.bottom()) <= 2;
}
}
