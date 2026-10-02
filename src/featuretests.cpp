#include "include/featuretests.h"
#include "include/awakemanager.h"
#include "include/clipboardocr.h"
#include "include/hotspotmanager.h"
#include "include/networkdetails.h"
#include "include/usbmanager.h"
#include <QFile>
#include <QGuiApplication>
#include <QTextStream>
#include <QTimer>
#include <QVariantMap>
#include <windows.h>
#include <appmodel.h>

int runFeatureTests(QGuiApplication &app, const QString &logPath)
{
    QFile file(logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return 20;
    QTextStream log(&file);
    int failures = 0;
    const auto check = [&](bool condition, const QString &name) {
        log << (condition ? "PASS " : "FAIL ") << name << '\n'; log.flush();
        if (!condition) ++failures;
    };
    quint64 now = 100;
    QList<QPair<bool, bool>> requests;
    bool allow = true;
    {
        AwakeManager awake(nullptr, [&](bool active, bool display) { requests.append({active, display}); return allow; }, [&] { return now; });
        awake.start(0, false); awake.start(241, true);
        check(requests.isEmpty() && !awake.active(), "awake rejects invalid duration without OS request");
        awake.start(1, false);
        check(awake.active() && !awake.keepDisplay() && awake.remainingSeconds() == 60, "awake system-only request");
        now += 1500; QMetaObject::invokeMethod(&awake, "tick", Qt::DirectConnection);
        check(awake.remainingSeconds() == 59, "awake monotonic countdown rounds up");
        awake.start(2, true);
        check(awake.keepDisplay() && awake.remainingSeconds() == 120, "awake replacement includes display");
        allow = false; awake.stop();
        check(awake.active(), "awake release failure keeps state for retry");
        allow = true; QMetaObject::invokeMethod(&awake, "tick", Qt::DirectConnection);
        check(!awake.active() && !requests.last().first, "awake release retries on timer");
        awake.start(1, false); now += 60000;
        QMetaObject::invokeMethod(&awake, "tick", Qt::DirectConnection);
        check(!awake.active() && awake.remainingSeconds() == 0, "awake expiry releases request");
        allow = false; awake.start(1, true);
        check(!awake.active(), "awake rejected OS request remains inactive");
        allow = true; awake.start(1, false);
    }
    check(!requests.last().first, "awake destructor releases request");
    check(HotspotManager::validConfiguration("test", "password123")
        && !HotspotManager::validConfiguration("", "password123")
        && !HotspotManager::validConfiguration(QString(33, 'x'), "password123")
        && !HotspotManager::validConfiguration(QString(11, QChar(0x4e2d)), "password123")
        && !HotspotManager::validConfiguration("test", "short")
        && !HotspotManager::validConfiguration("test", QString(64, 'x'))
        && !HotspotManager::validConfiguration("test", "bad\npassword"), "hotspot UTF-8 name and password validation");
    NetworkDetails network;
    HotspotManager hotspot;
    check(!hotspot.busy() && !hotspot.status().isEmpty(), "hotspot read-only capability snapshot; no toggle");
    log << "INFO hotspot supported " << hotspot.supported() << " status " << hotspot.status() << '\n'; log.flush();
    check(UsbManager::isStorageFunction("USB\\VID_1234&PID_5678\\test", "USBSTOR", true)
        && UsbManager::isStorageFunction("usb\\vid_1234&pid_5678\\test", "UASPStor", true)
        && !UsbManager::isStorageFunction("USB\\VID_1234&PID_5678\\test", "USBHUB3", true)
        && !UsbManager::isStorageFunction("USB\\ROOT_HUB30\\test", "USBSTOR", true)
        && !UsbManager::isStorageFunction("PCI\\test", "USBSTOR", true)
        && !UsbManager::isStorageFunction("USB\\VID_1234&PID_5678\\test", "USBSTOR", false), "USB target requires removable physical mass-storage function, never hub");
    bool safeFilters = true;
    for (int bits = 0; bits < 16; ++bits) {
        const bool actual = UsbManager::safeDisk(bits & 1, bits & 2, bits & 4, bits & 8);
        safeFilters &= actual == (bits == 3);
    }
    check(safeFilters, "USB all 16 disk protection combinations; system/boot/unknown/non-USB rejected");
    UsbManager usb;
    usb.eject("not-an-enumerated-device");
    check(!usb.busy() && usb.devices().isEmpty(), "USB unknown ID rejected before any eject request");
    ClipboardOcr ocr;
    ocr.recognize({}); ocr.finish(99, "stale", "stale"); ocr.copy({});
    check(!ocr.busy() && ocr.text().isEmpty() && ocr.status().isEmpty(), "OCR empty and stale requests ignored; no clipboard write");
    QStringList copied;
    ClipboardOcr copyProbe(nullptr, [&](const QString &value) { copied.append(value); });
    copyProbe.finish(0, "original fixture", "ready");
    int textSignals = 0;
    QObject::connect(&copyProbe, &ClipboardOcr::textChanged, &app, [&] { ++textSignals; });
    copyProbe.copy("edited fixture");
    copyProbe.copy({}); copyProbe.copy(QString(65537, 'x'));
    check(copied == QStringList{"edited fixture"} && copyProbe.text() == "original fixture" && textSignals == 0,
        "OCR copies edited text through mock sink without resetting editor; rejects empty/oversized copy");
    UINT32 length = 0;
    const bool packaged = GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
    bool ocrDone = !packaged, usbDone = false, networkDone = false;
    const QStringList fixtureKinds{"text", "blank", "malformed", "dimensions", "bytes"};
    int fixtureIndex = 0;
    if (!packaged) {
        ocr.recognize("fixture");
        check(!ocr.busy() && ocr.status().contains("MSIX"), "OCR unpackaged identity guard");
        log << "SKIP OCR real recognition: package identity required\n"; log.flush();
    }
    const auto finish = [&] {
        if (!ocrDone || !usbDone || !networkDone) return;
        log << (failures == 0 ? "FEATURE_TEST PASS" : "FEATURE_TEST FAIL") << '\n'; log.flush();
        app.exit(failures == 0 ? 0 : 21);
    };
    QObject::connect(&network, &NetworkDetails::changed, &app, [&] {
        if (networkDone || network.busy()) return;
        networkDone = true;
        bool valid = !network.summary().isEmpty();
        for (const auto &adapter : network.adapters()) {
            const auto data = adapter.toMap();
            valid &= !data.value("addresses").toString().isEmpty() && data.value("mtu").toUInt() > 0;
        }
        check(valid, "network asynchronous read-only adapter snapshot"); finish();
    });
    QObject::connect(&usb, &UsbManager::changed, &app, [&] {
        if (usb.busy() || usbDone) return;
        usbDone = true;
        bool valid = true;
        for (const auto &device : usb.devices()) {
            const auto data = device.toMap();
            valid &= data.value("id").toString().startsWith("USB\\VID_", Qt::CaseInsensitive)
                && !data.value("volumes").toString().isEmpty();
        }
        check(valid, "USB read-only enumeration safety filter; no physical eject");
        log << "INFO USB eligible count " << usb.devices().size() << '\n'; log.flush(); finish();
    });
    QObject::connect(&ocr, &ClipboardOcr::changed, &app, [&] {
        if (!packaged || ocr.busy() || ocrDone) return;
        const auto kind = fixtureKinds.at(fixtureIndex);
        if (kind == "text") check(ocr.text().contains("OCR", Qt::CaseInsensitive) && ocr.text().contains("123"), "OCR generated fixture recognized via production pipeline");
        else if (kind == "blank") check(ocr.text().isEmpty() && ocr.status().contains(QStringLiteral("未识别到文字")), "OCR blank image returns empty result, not stale text");
        else if (kind == "malformed") check(ocr.text().isEmpty() && ocr.status().contains(QStringLiteral("失败")), "OCR malformed image reports failure without crashing");
        else if (kind == "dimensions") check(ocr.text().isEmpty() && ocr.status().contains(QStringLiteral("尺寸限制")), "OCR oversized dimensions rejected before bitmap recognition");
        else if (kind == "bytes") check(ocr.text().isEmpty() && ocr.status().contains("12 MB"), "OCR stream byte limit enforced before decoder");
        if (++fixtureIndex == fixtureKinds.size()) { ocrDone = true; finish(); }
        else QTimer::singleShot(0, &app, [&] { ocr.recognizeFixture(fixtureKinds.at(fixtureIndex)); });
    });
    QTimer::singleShot(0, &app, [&] { if (packaged) ocr.recognizeFixture(); });
    QTimer::singleShot(20000, &app, [&] { log << "FAIL feature test timeout\n"; log.flush(); app.exit(22); });
    return app.exec();
}
