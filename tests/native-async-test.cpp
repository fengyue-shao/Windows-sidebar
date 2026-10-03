#include "include/clipboardocr.h"
#include "include/hotspotmanager.h"
#include <QCoreApplication>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>

#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)

// Local WinRT coroutine only: no clipboard, OCR engine, network or radio API.
static winrt::Windows::Foundation::IAsyncAction pendingAction(std::shared_ptr<std::atomic<bool>> canceled)
{
    auto token = co_await winrt::get_cancellation_token();
    token.callback([canceled] { canceled->store(true); });
    co_await winrt::resume_after(std::chrono::milliseconds(100));
}

class NativeAsyncTest {
public:
    static int run() {
        QStringList copies;
        ClipboardOcr ocr(nullptr, [&](const QString &text) { copies.append(text); });
        int completed = 0;
        QObject::connect(&ocr, &ClipboardOcr::completed, [&] { ++completed; });
        auto generation = ocr.beginRecognition("working");
        auto canceled = std::make_shared<std::atomic<bool>>(false);
        ocr.m_action = pendingAction(canceled);
        ocr.clear(); ocr.copy("busy copy");
        CHECK(ocr.busy() && copies.isEmpty());
        ocr.timeout(generation - 1); CHECK(ocr.busy());
        ocr.cancel(); CHECK(!ocr.busy() && canceled->load() && completed == 0);
        const auto canceledStatus = ocr.status();
        ocr.finish(generation, "late", "late"); CHECK(ocr.text().isEmpty() && ocr.status() == canceledStatus);
        generation = ocr.beginRecognition("new");
        ocr.finish(generation - 1, "old", "old"); CHECK(ocr.busy());
        ocr.finish(generation, "result", "done"); CHECK(!ocr.busy() && ocr.text() == "result" && completed == 1);
        ocr.finish(generation, "duplicate", "duplicate"); CHECK(ocr.text() == "result" && completed == 1);
        ocr.copy("edited"); CHECK(copies == QStringList{"edited"});
        ocr.clear(); CHECK(ocr.text().isEmpty() && ocr.status().isEmpty());
        generation = ocr.beginRecognition("timeout"); ocr.timeout(generation);
        CHECK(!ocr.busy() && ocr.status().contains("30") && completed == 1);
        ocr.finish(generation, "late timeout", "late"); CHECK(ocr.text().isEmpty());
        auto destructorCanceled = std::make_shared<std::atomic<bool>>(false);
        { ClipboardOcr scoped; scoped.beginRecognition("exit"); scoped.m_action = pendingAction(destructorCanceled); }
        CHECK(destructorCanceled->load());

        HotspotManager hotspot(nullptr, false);
        int hotspotCompleted = 0;
        QObject::connect(&hotspot, &HotspotManager::operationCompleted, [&] { ++hotspotCompleted; });
        generation = hotspot.beginOperation("working");
        canceled = std::make_shared<std::atomic<bool>>(false); hotspot.m_action = pendingAction(canceled);
        hotspot.timeout(generation - 1); CHECK(hotspot.busy());
        hotspot.cancel(); CHECK(!hotspot.busy() && canceled->load() && hotspotCompleted == 0);
        const auto stoppedStatus = hotspot.status();
        hotspot.finished(generation, "late"); CHECK(hotspot.status() == stoppedStatus);
        generation = hotspot.beginOperation("new");
        hotspot.finished(generation - 1, "old"); CHECK(hotspot.busy());
        hotspot.finished(generation, "done"); CHECK(!hotspot.busy() && hotspotCompleted == 1 && hotspot.status() == "done");
        hotspot.finished(generation, "duplicate"); CHECK(hotspotCompleted == 1 && hotspot.status() == "done");
        generation = hotspot.beginOperation("timeout"); hotspot.timeout(generation);
        CHECK(!hotspot.busy() && hotspot.status().contains("45") && hotspotCompleted == 1);
        hotspot.finished(generation, "late timeout"); CHECK(hotspot.status().contains("45"));
        destructorCanceled = std::make_shared<std::atomic<bool>>(false);
        { HotspotManager scoped(nullptr, false); scoped.beginOperation("exit"); scoped.m_action = pendingAction(destructorCanceled); }
        CHECK(destructorCanceled->load());
        hotspot.configure("", "short"); CHECK(!hotspot.busy() && hotspot.status().contains("UTF-8"));
        std::cout << "PASS OCR/hotspot lifecycle: cancel requests, generation guards, completion once, timeout, shutdown, mock copy and validation; no system feature APIs\n";
        return 0;
    }
};
int main(int argc, char **argv) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    QCoreApplication app(argc, argv);
    return NativeAsyncTest::run();
}
