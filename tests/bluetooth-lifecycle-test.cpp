#include "include/bluetoothmanager.h"
#include <QCoreApplication>
#include <iostream>
class BluetoothLifecycleTest {
public:
    static int run() {
        BluetoothManager manager;
        manager.m_devices = {QVariantMap{{"id", "old"}}};
        manager.m_scanning = true; manager.m_scanGeneration = 5;
        manager.cancelScan();
        manager.acceptDevice(0, {{"id", "late"}, {"name", "late"}}, 5);
        if (manager.scanning() || manager.devices().at(0).toMap().value("id") != "old") return 1;
        manager.m_scanning = true; manager.m_scanGeneration = 7;
        manager.m_previousRows.insert("old", {{"id", "old"}, {"name", "old"}});
        manager.acceptDevice(0, {{"id", "current"}, {"name", "current"}}, 7);
        if (!manager.scanning() || manager.devices().size() != 2) return 2;
        // Four initial enumerations reconcile previous rows, but keep discovery active.
        for (size_t i = 0; i < 3; ++i) manager.enumerationComplete(i, 7);
        if (manager.devices().size() != 2) return 6;
        manager.enumerationComplete(3, 7);
        if (!manager.scanning() || manager.devices().size() != 1 || manager.devices().at(0).toMap().value("id") != "current") return 7;
        manager.acceptDevice(1, {{"id", "new"}, {"name", "new"}, {"connected", true}}, 7);
        if (manager.devices().size() != 2 || manager.devices().at(0).toMap().value("id") != "new") return 8;
        // Same ID from another watcher is not duplicated and survives one source removal.
        manager.acceptDevice(2, {{"id", "new"}, {"name", "renamed"}, {"paired", true}}, 7);
        manager.removeDevice(1, "new", 7);
        if (manager.devices().size() != 2 || manager.devices().at(0).toMap().value("name") != "renamed") return 9;
        manager.removeDevice(2, "new", 7);
        manager.acceptDevice(0, {{"id", "current"}, {"name", "updated"}, {"connected", true}}, 7);
        manager.acceptDevice(0, {{"id", "no-name"}, {"name", ""}}, 7);
        manager.acceptDevice(99, {{"id", "bad"}, {"name", "bad"}}, 7);
        if (manager.devices().size() != 1 || manager.devices().at(0).toMap().value("name") != "updated") return 10;
        manager.finishScan(6);
        if (!manager.scanning()) return 11;
        manager.finishScan(7);
        const auto finalDevices = manager.devices();
        manager.removeDevice(0, "current", 7);
        manager.enumerationComplete(0, 7);
        manager.watcherFailed(7, "stale failure");
        if (manager.scanning() || manager.devices() != finalDevices || !manager.statusText().startsWith("扫描结束")) return 12;
        manager.m_scanning = true; manager.m_scanGeneration = 9;
        manager.m_watchers[0].complete = false;
        manager.finishScan(9);
        if (manager.scanning() || !manager.statusText().contains("未全部完成")) return 13;
        manager.m_pairing = true; manager.m_pairGeneration = 8;
        manager.cancelOperation();
        const auto status = manager.statusText();
        manager.applyOperation("stale success", 8);
        if (manager.busy() || manager.statusText() != status) return 3;
        manager.m_pairing = true; manager.m_pairGeneration = 10;
        manager.applyOperation("current completion", 10);
        if (manager.busy() || manager.statusText() != "current completion") return 4;
        // Queued post-operation scan is owned by manager and never run in this test.
        manager.pairDevice(""); manager.unpairDevice(""); if (manager.busy()) return 5;
        std::cout << "PASS Bluetooth lifecycle: incremental add/update/remove, source dedup, initial reconciliation, continued discovery, 30-second finish/incomplete state, cancellation/stale guards, pairing busy; no discovery/pairing APIs\n";
        return 0;
    }
};
int main(int argc, char **argv) { QCoreApplication app(argc, argv); return BluetoothLifecycleTest::run(); }
