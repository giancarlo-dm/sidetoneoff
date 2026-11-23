// The tray app: the hidden window, its messages and the state machine (plan, section 5.2).
#pragma once

#include <windows.h>

#include <memory>

#include "devices.h"
#include "log_window.h"
#include "tray.h"
#include "worker.h"

class App {
public:
    // Creates the window, the tray icon, the device notifications and the worker, shows the "Sidetone Off is
    // running" notice if `showNotice`, and starts turning sidetone off if the headset is already plugged in.
    // nullptr (logged) on failure.
    static std::unique_ptr<App> Create(HINSTANCE instance, bool showNotice);
    ~App();

    // Not copyable or movable: the window holds a pointer to this object.
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // The message loop. Returns the exit code once the window is destroyed (Quit, Uninstall, or WM_CLOSE).
    int Run();

    // True if the app ended through Tray > Uninstall…. It is unregistered already, but its files can only be
    // deleted once it has stopped: the caller does that last (RemoveInstalledFiles).
    bool Uninstalled() const { return uninstalled_; }

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
    void Uninstall();  // Tray > Uninstall…: asks, unregisters, then ends the app

    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    HeadsetStatus status_ = HeadsetStatus::Disconnected;
    // A dialog runs further up the call stack while inDialog_ is set; dialog_ is its window once it exists.
    bool inDialog_ = false;
    HWND dialog_ = nullptr;
    bool uninstalled_ = false;
    // Empty until Create fills them, and emptied again when the window is destroyed (WM_DESTROY).
    std::unique_ptr<Tray> tray_;
    std::unique_ptr<Worker> worker_;
    std::unique_ptr<DeviceWatcher> devices_;
    std::unique_ptr<LogWindow> logWindow_;  // Tray > Show log; its window exists only while open
};
