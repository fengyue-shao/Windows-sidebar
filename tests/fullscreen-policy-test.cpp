#include "include/fullscreenpolicy.h"
#include <iostream>
int main() {
    using SidebarFullscreen::coversMonitor;
    const QRect monitor(0,0,1920,1080), second(-1920,0,1920,1080);
    if (!coversMonitor(monitor, monitor) || !coversMonitor(second, second)
        || coversMonitor(second, monitor) || coversMonitor(QRect(0,0,1920,1040), monitor)
        || coversMonitor(QRect(), monitor) || coversMonitor(QRect(0,0,3840,1080), monitor)
        || !coversMonitor(QRect(-1,-1,1922,1082), monitor)) return 1;
    std::cout << "PASS fullscreen geometry: exact, border tolerance, negative monitor coordinates, other monitor, taskbar work area, empty and spanning window; no screen/window APIs\n";
    return 0;
}
