#include "include/hotkeypolicy.h"
#include <iostream>
int main() {
    using namespace SidebarHotkey;
    if (!parse("Ctrl+Alt+Space").valid || !parse("Ctrl+Shift+F12").valid || !parse("Meta+K").valid
        || parse("A").valid || parse("Shift+A").valid || parse("Ctrl+A, Ctrl+B").valid || parse("Ctrl+Tab").valid) return 1;
    int adds=0, removes=0, undoes=0;
    auto result = replace(true, [&]{++adds; return DWORD{1409};}, [&]{++removes; return DWORD{0};}, [&]{++undoes;});
    if (result.accepted || removes || undoes || adds != 1) return 2;
    result = replace(true, [&]{return DWORD{0};}, [&]{++removes; return DWORD{5};}, [&]{++undoes;});
    if (result.accepted || result.error != 5 || undoes != 1) return 3;
    result = replace(true, [&]{return DWORD{0};}, [&]{return DWORD{0};}, [&]{++undoes;});
    if (!result.accepted || undoes != 1) return 4;
    std::cout << "PASS hotkey policy: supported parsing, invalid/multiple sequences, conflict retains old, old removal failure undoes candidate, successful swap; no registrations\n";
    return 0;
}
