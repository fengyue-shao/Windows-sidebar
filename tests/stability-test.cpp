#include "include/boundedasync.h"
#include <iostream>
#include <memory>
using winrt::Windows::Foundation::AsyncStatus;
struct State { AsyncStatus status; int waits = 0, results = 0, cancels = 0; };
struct Operation {
    std::shared_ptr<State> state;
    AsyncStatus wait_for(std::chrono::milliseconds) { ++state->waits; return state->status; }
    void Cancel() { ++state->cancels; }
    int GetResults() {
        ++state->results;
        if (state->status != AsyncStatus::Completed) throw winrt::hresult_canceled();
        return 42;
    }
};
int main()
{
    for (auto status : {AsyncStatus::Completed, AsyncStatus::Started, AsyncStatus::Canceled, AsyncStatus::Error}) {
        auto state = std::make_shared<State>(State{status});
        bool threw = false;
        try { if (SidebarAsync::result(Operation{state}, false) != 42) return 1; }
        catch (const winrt::hresult_error &) { threw = true; }
        if (threw != (status != AsyncStatus::Completed) || state->waits != 1
            || state->results != (status == AsyncStatus::Started ? 0 : 1)
            || state->cancels != (status == AsyncStatus::Started ? 1 : 0)) return 2;
    }
    auto state = std::make_shared<State>(State{AsyncStatus::Completed});
    try { SidebarAsync::result(Operation{state}, true); return 3; }
    catch (const winrt::hresult_error &) {}
    if (state->waits || state->results || state->cancels != 1) return 4;
    std::cout << "STABILITY PASS: success/error/cancel/timeout/shutdown; one waiter; no system APIs invoked\n";
}
