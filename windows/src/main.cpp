// Sidetone Off for Windows: turns off the Logitech G635's sidetone whenever the headset is connected.
// Lives in the system tray and starts at sign-in.

#include <windows.h>

#include "log.h"
#include "strings.h"
#include "version.h"

namespace {

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CLOSE:  // sent by taskkill (without /f) and, later, by install/uninstall
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

// A hidden top-level window, never shown. Not HWND_MESSAGE: message-only windows don't get
// broadcasts, and the tray needs the TaskbarCreated broadcast to come back after Explorer restarts.
HWND CreateHiddenWindow(HINSTANCE instance) {
    const WNDCLASSEXW windowClass{
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = WindowProc,
        .hInstance = instance,
        .lpszClassName = ids::kWindowClass,
    };
    if (!RegisterClassExW(&windowClass)) return nullptr;
    return CreateWindowExW(0, ids::kWindowClass, ui::kAppName, WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr,
                           instance, nullptr);
}

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, ids::kMutex);
    if (!mutex) {
        Log(L"can't create the single-instance mutex (error %lu)", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Log(L"already running, exiting");
        CloseHandle(mutex);
        return 0;
    }

    HWND window = CreateHiddenWindow(instance);
    if (!window) {
        Log(L"can't create the window (error %lu)", GetLastError());
        CloseHandle(mutex);
        return 1;
    }
    Log(L"started, version %hs", SIDETONE_VERSION_STRING);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Log(L"exiting");
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
