#include "devices.h"

#include <initguid.h>  // must come before usbiodef.h so GUID_DEVINTERFACE_USB_DEVICE is defined here
#include <usbiodef.h>
#include <dbt.h>

#include <utility>

#include "hid_windows.h"
#include "log.h"

namespace {

UniqueDeviceNotification Register(HWND window, const GUID& interfaceClass) {
    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = interfaceClass;
    return UniqueDeviceNotification(RegisterDeviceNotificationW(window, &filter, DEVICE_NOTIFY_WINDOW_HANDLE));
}

// Asks the system rather than keeping a count, so the state can't drift if a notification is missed.
bool AnyG635Plugged() {
    const auto devices = FindG635Interfaces(GUID_DEVINTERFACE_USB_DEVICE);
    return devices && !devices->empty();
}

}  // namespace

std::unique_ptr<DeviceWatcher> DeviceWatcher::Create(HWND window) {
    const GUID hidClass = HidInterfaceClass();
    UniqueDeviceNotification usb = Register(window, GUID_DEVINTERFACE_USB_DEVICE);
    UniqueDeviceNotification hid = Register(window, hidClass);
    if (!usb || !hid) {
        Log(L"can't register for device notifications (error %lu)", GetLastError());
        return nullptr;
    }
    // Registered first, then checked: a G635 plugged in between is seen by both, which is harmless.
    std::unique_ptr<DeviceWatcher> watcher(new DeviceWatcher(std::move(usb), std::move(hid), hidClass));
    watcher->connected_ = AnyG635Plugged();
    Log(L"G635 %s at startup", watcher->connected_ ? L"connected" : L"not connected");
    return watcher;
}

DeviceWatcher::DeviceWatcher(UniqueDeviceNotification usb, UniqueDeviceNotification hid, GUID hidClass)
    : usb_(std::move(usb)), hid_(std::move(hid)), hidClass_(hidClass) {}

DeviceChange DeviceWatcher::OnDeviceChange(WPARAM event, LPARAM data) {
    if (event != DBT_DEVICEARRIVAL && event != DBT_DEVICEREMOVECOMPLETE) return DeviceChange::None;
    // lParam points to a header whose type says which struct it really is.
    const auto* header = reinterpret_cast<const DEV_BROADCAST_HDR*>(data);
    if (header == nullptr || header->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) return DeviceChange::None;
    const auto* device = reinterpret_cast<const DEV_BROADCAST_DEVICEINTERFACE_W*>(data);
    if (!IsG635Path(device->dbcc_name)) return DeviceChange::None;

    const bool isUsb = IsEqualGUID(device->dbcc_classguid, GUID_DEVINTERFACE_USB_DEVICE);
    const bool isHid = IsEqualGUID(device->dbcc_classguid, hidClass_);
    if (event == DBT_DEVICEARRIVAL) {
        if (isUsb) {
            connected_ = true;
            return DeviceChange::Connected;
        }
        if (isHid) return DeviceChange::HidArrived;
    } else if (isUsb) {
        connected_ = AnyG635Plugged();  // another G635 may still be there
        if (!connected_) return DeviceChange::Disconnected;
    }
    return DeviceChange::None;
}
