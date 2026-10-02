#pragma once
#include <chrono>
#include <winrt/Windows.Foundation.h>

namespace SidebarAsync {
// A WinRT operation supports one waiter: wait_for + GetResults, not wait_for + get.
template<typename Operation>
auto result(Operation operation, bool interrupted, std::chrono::milliseconds timeout = std::chrono::seconds(3))
{
    if (interrupted) { operation.Cancel(); throw winrt::hresult_canceled(); }
    if (operation.wait_for(timeout) == winrt::Windows::Foundation::AsyncStatus::Started) {
        operation.Cancel(); // Best effort; never wait for cancellation acknowledgement.
        throw winrt::hresult_error(winrt::hresult{static_cast<int32_t>(0x800705b4u)});
    }
    return operation.GetResults(); // Propagates Canceled/Error as well as success.
}
}
