// Plug and unplug detection through WM_DEVICECHANGE, without polling (plan, section 5.2).
#pragma once

#include <windows.h>

#include <memory>

#include "win32.h"

// What a WM_DEVICECHANGE message means for the app.
enum class DeviceChange {
    None,          // not the G635, or nothing that matters
    Connected,     // the G635's USB device arrived
    Disconnected,  // the last G635 USB device went away
    HidArrived,    // one of the G635's HID collections arrived (they come up shortly after the USB device)
};

class DeviceWatcher {
public:
    // Registers `window` for USB-device and HID-interface notifications, and checks whether a G635 is
    // plugged in already. nullptr (logged) if the registration fails.
    static std::unique_ptr<DeviceWatcher> Create(HWND window);

    // Call with every WM_DEVICECHANGE. Updates IsConnected() and says what changed.
    DeviceChange OnDeviceChange(WPARAM event, LPARAM data);

    bool IsConnected() const { return connected_; }

private:
    DeviceWatcher(UniqueDeviceNotification usb, UniqueDeviceNotification hid, GUID hidClass);

    UniqueDeviceNotification usb_;
    UniqueDeviceNotification hid_;
    GUID hidClass_;
    bool connected_ = false;
};
