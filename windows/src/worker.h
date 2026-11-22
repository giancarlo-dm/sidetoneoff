// The worker thread: applies the profile to the headset, with retries, off the UI thread (plan, section 5.2).
#pragma once

#include <windows.h>

#include <memory>
#include <thread>

#include "profile.h"
#include "win32.h"

class Worker {
public:
    // Starts the thread. After each run it posts `resultMessage` to `window`, with wParam 1 when the headset
    // confirmed the profile and 0 when every attempt failed. nullptr (logged) if it can't start.
    static std::unique_ptr<Worker> Start(HWND window, UINT resultMessage, const Profile& profile);

    // Stops the thread. An attempt in progress finishes first; that takes milliseconds, a few seconds at
    // most if the headset doesn't answer.
    ~Worker();

    // Not copyable or movable: the thread holds a pointer to this object.
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // Asks for a run. Requests that arrive before the run starts merge into it; one that arrives during a
    // run causes one more run afterwards.
    void RequestApply();

private:
    Worker(HWND window, UINT resultMessage, const Profile& profile, UniqueHandle wake, UniqueHandle stop);

    void Run();  // the thread's body

    HWND window_;
    UINT resultMessage_;
    Profile profile_;
    UniqueHandle wake_;  // auto-reset event: set by RequestApply()
    UniqueHandle stop_;  // manual-reset event: set by the destructor
    std::jthread thread_;  // declared last: it starts after the members above exist, and is joined first
};
