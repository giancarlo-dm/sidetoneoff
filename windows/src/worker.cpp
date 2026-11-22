#include "worker.h"

#include <iterator>
#include <utility>

#include "hid_windows.h"
#include "hidpp.h"
#include "log.h"

namespace {

constexpr int kAttempts = 5;
constexpr DWORD kWaitBeforeAttemptMs = 1000;  // the Mac's 1 s wait

// One attempt: open the headset, apply the profile, close it again.
std::expected<void, hidpp::Error> ApplyOnce(const Profile& profile) {
    auto transport = WinHidTransport::Open();
    if (!transport) return std::unexpected(transport.error());
    hidpp::Device device{*transport};
    return Apply(device, profile);
}  // the transport closes the device here

}  // namespace

std::unique_ptr<Worker> Worker::Start(HWND window, UINT resultMessage, const Profile& profile) {
    UniqueHandle wake{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    UniqueHandle stop{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!wake || !stop) {
        Log(L"can't create the worker's events (error %lu)", GetLastError());
        return nullptr;
    }
    // `new` because the constructor is private, which std::make_unique can't reach.
    return std::unique_ptr<Worker>(new Worker(window, resultMessage, profile, std::move(wake), std::move(stop)));
}

Worker::Worker(HWND window, UINT resultMessage, const Profile& profile, UniqueHandle wake, UniqueHandle stop)
    : window_(window),
      resultMessage_(resultMessage),
      profile_(profile),
      wake_(std::move(wake)),
      stop_(std::move(stop)),
      thread_([this] { Run(); }) {}  // a lambda: the code the new thread runs

Worker::~Worker() {
    SetEvent(stop_.get());
}  // then thread_ is destroyed, which waits for Run() to return

void Worker::RequestApply() {
    SetEvent(wake_.get());
}

void Worker::Run() {
    const HANDLE events[] = {stop_.get(), wake_.get()};  // stop first: it wins when both are set
    // Waits up to `timeoutMs` for a request. False when the worker must stop instead. A lambda: a local
    // function; [&events] lets it use that local variable.
    const auto waitForRequest = [&events](DWORD timeoutMs) {
        const DWORD result = WaitForMultipleObjects(static_cast<DWORD>(std::size(events)), events, FALSE, timeoutMs);
        return result == WAIT_OBJECT_0 + 1 || result == WAIT_TIMEOUT;
    };

    while (waitForRequest(INFINITE)) {
        bool off = false;
        for (int attempt = 1; attempt <= kAttempts && !off; ++attempt) {
            // A new request (e.g. the HID collection arriving) cuts this wait short.
            if (!waitForRequest(kWaitBeforeAttemptMs)) return;
            const auto result = ApplyOnce(profile_);
            if (result) {
                off = true;
                Log(L"sidetone off (attempt %d)", attempt);
            } else {
                Log(L"attempt %d failed: %hs", attempt, hidpp::Describe(result.error()).c_str());
            }
        }
        if (!off) Log(L"couldn't turn off sidetone after %d attempts", kAttempts);
        PostMessageW(window_, resultMessage_, off ? 1 : 0, 0);
    }
}
