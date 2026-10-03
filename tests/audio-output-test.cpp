#include "include/audiooutputswitch.h"
#include <QCoreApplication>
#include <array>
#include <iostream>

#define CHECK(value) do { if (!(value)) { std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString wanted = QStringLiteral("fixture-output");
    std::array<HRESULT, 3> writes{S_OK, S_OK, S_OK};
    std::array<SidebarAudio::EndpointRead, 3> reads{{{S_OK, wanted}, {S_OK, wanted}, {S_OK, wanted}}};
    int writeCalls = 0, readCalls = 0;
    auto run = [&](const QString &id = QStringLiteral("fixture-output")) {
        writeCalls = readCalls = 0;
        return SidebarAudio::switchOutput(id,
            [&](ERole role) { ++writeCalls; return writes.at(size_t(role)); },
            [&](ERole role) { ++readCalls; return reads.at(size_t(role)); });
    };
    auto outcome = run(QString());
    CHECK(!outcome.confirmed && !writeCalls && !readCalls);
    outcome = run();
    CHECK(outcome.confirmed && outcome.matchingRoles == 3 && writeCalls == 3 && readCalls == 3);
    writes[1] = E_ACCESSDENIED;
    reads[1].deviceId = QStringLiteral("other-device");
    outcome = run();
    CHECK(!outcome.confirmed && outcome.matchingRoles == 2 && writeCalls == 3 && readCalls == 3);
    CHECK(outcome.message.contains(QStringLiteral("0x80070005")));
    // A failed request must remain visible even when already selected.
    reads[1].deviceId = wanted;
    CHECK(!run().confirmed);
    writes.fill(S_OK);
    reads[2] = {E_FAIL, QString()};
    outcome = run();
    CHECK(!outcome.confirmed && outcome.matchingRoles == 2 && outcome.message.contains(QStringLiteral("0x80004005")));
    reads[2] = {S_OK, QStringLiteral("other-device")};
    CHECK(!run().confirmed);
    reads[2] = {S_OK, wanted};
    writes[0] = S_FALSE;
    CHECK(run().confirmed);
    writes.fill(E_FAIL);
    for (auto &read : reads) read = {S_OK, QStringLiteral("other-device")};
    outcome = run();
    CHECK(!outcome.confirmed && outcome.matchingRoles == 0 && writeCalls == 3 && readCalls == 3);
    std::cout << "PASS audio output: invalid ID, three roles, partial/request failure, read failure, mismatch, HRESULT success; no system APIs invoked\n";
    return 0;
}
