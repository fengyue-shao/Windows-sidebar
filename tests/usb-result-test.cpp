#include "include/usbmanager.h"
#include "include/usbejectresult.h"
#include <QCoreApplication>
#include <iostream>
#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << '\n'; return 1; } } while (false)
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    for (int veto = PNP_VetoTypeUnknown; veto <= PNP_VetoAlreadyRemoved; ++veto) {
        const auto message = SidebarUsb::ejectMessage(CR_REMOVE_VETOED, static_cast<PNP_VETO_TYPE>(veto), "fixture-object");
        CHECK(message.contains("未强制移除") && message.contains("fixture-object") && message.contains(QString::number(CR_REMOVE_VETOED)));
    }
    CHECK(SidebarUsb::ejectMessage(CR_REMOVE_VETOED, PNP_VetoOutstandingOpen, {}).contains("占用"));
    CHECK(SidebarUsb::ejectMessage(CR_REMOVE_VETOED, PNP_VetoInsufficientRights, {}).contains("权限不足"));
    CHECK(SidebarUsb::ejectMessage(CR_SUCCESS, PNP_VetoOutstandingOpen, "ignored").contains("接受"));
    CHECK(!SidebarUsb::ejectMessage(CR_SUCCESS, PNP_VetoOutstandingOpen, "ignored").contains("ignored"));
    CHECK(SidebarUsb::ejectMessage(CR_FAILURE, static_cast<PNP_VETO_TYPE>(99), {}).contains("99"));
    for (int bits = 0; bits < 16; ++bits) CHECK(UsbManager::safeDisk(bits&1, bits&2, bits&4, bits&8) == (bits == 3));
    CHECK(UsbManager::isStorageFunction("USB\\VID_1234&PID_5678\\fixture", "USBSTOR", true));
    CHECK(!UsbManager::isStorageFunction("USB\\ROOT_HUB30\\fixture", "USBHUB3", true));
    UsbManager manager(nullptr, false); // No startup enumeration.
    int operations = 0; bool success = true;
    QObject::connect(&manager, &UsbManager::operationFinished, [&](bool value) { ++operations; success = value; });
    const QVariantList rows{QVariantMap{{"id", "fixture"}, {"name", "fixture storage"}, {"volumes", "Z:\\"}}};
    manager.finish(rows, "ready");
    manager.eject("unknown"); CHECK(!manager.busy() && manager.devices() == rows && operations == 0);
    manager.finish(rows, SidebarUsb::ejectMessage(CR_REMOVE_VETOED, PNP_VetoOutstandingOpen, "fixture"), -1);
    CHECK(manager.devices() == rows && operations == 1 && !success && !manager.busy());
    manager.finish({}, SidebarUsb::ejectMessage(CR_SUCCESS, PNP_VetoTypeUnknown, {}), 1);
    CHECK(manager.devices().isEmpty() && operations == 2 && success);
    std::cout << "PASS USB result: veto reasons, raw code, related object, occupied failure retains list, safety filters and unknown ID; no enumeration/eject APIs\n";
    return 0;
}
