// The tray app: the hidden window, its messages and the state machine (plan, section 5.2).
#pragma once

#include <windows.h>

#include <memory>

#include "devices.h"
#include "tray.h"
#include "worker.h"

class App {
public:
    // Creates the window, the tray icon, the device notifications and the worker, and starts turning
    // sidetone off if the headset is already plugged in. nullptr (logged) on failure.
    static std::unique_ptr<App> Create(HINSTANCE instance);
    ~App();

    // Not copyable or movable: the window holds a pointer to this object.
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // The message loop. Returns the exit code once the window is destroyed (Quit, or WM_CLOSE).
    int Run();

private:
    App() = default;

    // Windows calls this static function; it forwards to HandleMessage on the App the window belongs to.
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void OnDeviceChange(WPARAM event, LPARAM data);
    void OnWorkerResult(bool off);
    void OnTrayEvent(UINT event, POINT anchor);
    void TurnOffSidetone();  // shows "turning off sidetone…" and asks the worker for a run
    void SetStatus(HeadsetStatus status);

    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    HeadsetStatus status_ = HeadsetStatus::Disconnected;
    // Empty until Create fills them, and emptied again when the window is destroyed (WM_DESTROY).
    std::unique_ptr<Tray> tray_;
    std::unique_ptr<Worker> worker_;
    std::unique_ptr<DeviceWatcher> devices_;
};
