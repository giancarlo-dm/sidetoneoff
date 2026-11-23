// Small RAII helpers for Win32. A wrapper releases its handle when it goes out of scope, on every path,
// including early returns: like Java's try-with-resources, but with no keyword (plan, section 5.0).
#pragma once

#include <windows.h>

#include <iterator>
#include <memory>
#include <string>
#include <type_traits>

// Closes a kernel handle (file, event, mutex, thread). `operator()` lets an object be called like a
// function; std::unique_ptr calls it when it lets go of the handle.
struct HandleCloser {
    void operator()(HANDLE handle) const noexcept { CloseHandle(handle); }
};

// Owns a HANDLE and closes it automatically. Move-only: ownership can be handed over (std::move) but
// never copied. `.get()` gives the raw handle for API calls; an empty one tests false (`if (!handle)`).
using UniqueHandle = std::unique_ptr<void, HandleCloser>;

// CreateFileW reports failure as INVALID_HANDLE_VALUE, not null; this turns that into an empty UniqueHandle.
inline UniqueHandle OwnFileHandle(HANDLE handle) {
    return UniqueHandle(handle == INVALID_HANDLE_VALUE ? nullptr : handle);
}

// An open registry key, closed automatically.
struct HkeyCloser {
    void operator()(HKEY key) const noexcept { RegCloseKey(key); }
};
using UniqueHkey = std::unique_ptr<std::remove_pointer_t<HKEY>, HkeyCloser>;

// A RegisterDeviceNotificationW registration, unregistered automatically.
struct DeviceNotificationCloser {
    void operator()(HDEVNOTIFY notification) const noexcept { UnregisterDeviceNotification(notification); }
};
using UniqueDeviceNotification = std::unique_ptr<void, DeviceNotificationCloser>;

// A menu, destroyed automatically. HMENU points to an opaque struct, hence the remove_pointer_t.
struct MenuDestroyer {
    void operator()(HMENU menu) const noexcept { DestroyMenu(menu); }
};
using UniqueMenu = std::unique_ptr<std::remove_pointer_t<HMENU>, MenuDestroyer>;

// An icon made with CreateIconIndirect, destroyed automatically.
struct IconDestroyer {
    void operator()(HICON icon) const noexcept { DestroyIcon(icon); }
};
using UniqueIcon = std::unique_ptr<std::remove_pointer_t<HICON>, IconDestroyer>;

// A GDI bitmap, deleted automatically. GDI can't delete a bitmap that is still selected into a DC.
struct GdiObjectDeleter {
    void operator()(HGDIOBJ object) const noexcept { DeleteObject(object); }
};
using UniqueBitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, GdiObjectDeleter>;

// A memory DC (CreateCompatibleDC), deleted automatically.
struct DcDeleter {
    void operator()(HDC dc) const noexcept { DeleteDC(dc); }
};
using UniqueDc = std::unique_ptr<std::remove_pointer_t<HDC>, DcDeleter>;

// The system's text for an error code, e.g. 2 -> "The system cannot find the file specified."
inline std::wstring SystemErrorText(DWORD error) {
    wchar_t text[512];
    DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, text,
                                  static_cast<DWORD>(std::size(text)), nullptr);
    while (length > 0 && (text[length - 1] == L'\r' || text[length - 1] == L'\n')) --length;
    return std::wstring(text, length);
}
