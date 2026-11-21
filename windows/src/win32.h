// Small RAII helpers for Win32. A wrapper releases its handle when it goes out of scope, on every path,
// including early returns: like Java's try-with-resources, but with no keyword (plan, section 5.0).
#pragma once

#include <windows.h>

#include <iterator>
#include <memory>
#include <string>

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

// The system's text for an error code, e.g. 2 -> "The system cannot find the file specified."
inline std::wstring SystemErrorText(DWORD error) {
    wchar_t text[512];
    DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, text,
                                  static_cast<DWORD>(std::size(text)), nullptr);
    while (length > 0 && (text[length - 1] == L'\r' || text[length - 1] == L'\n')) --length;
    return std::wstring(text, length);
}
