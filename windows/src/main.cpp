// Sidetone Off for Windows: turns off the Logitech G635's sidetone whenever the headset is connected.
// Lives in the system tray and starts at sign-in.

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "diag.h"
#include "log.h"
#include "strings.h"
#include "version.h"
#include "win32.h"

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

// The tray app (no arguments). For now: the single-instance check, the hidden window and the message loop.
int RunTray(HINSTANCE instance) {
    const UniqueHandle mutex{CreateMutexW(nullptr, FALSE, ids::kMutex)};
    const DWORD mutexError = GetLastError();
    if (!mutex) {
        Log(L"can't create the single-instance mutex (error %lu)", mutexError);
        return 1;
    }
    if (mutexError == ERROR_ALREADY_EXISTS) {
        Log(L"already running, exiting");
        return 0;
    }

    if (!CreateHiddenWindow(instance)) {
        Log(L"can't create the window (error %lu)", GetLastError());
        return 1;
    }
    Log(L"started, version %hs", SIDETONE_VERSION_STRING);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Log(L"exiting");
    return static_cast<int>(message.wParam);
}  // the mutex is closed here, on every return path above

// The command-line arguments, without the program name.
std::vector<std::wstring> Arguments() {
    int count = 0;
    wchar_t** list = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!list) return {};
    std::vector<std::wstring> arguments;
    for (int i = 1; i < count; ++i) arguments.emplace_back(list[i]);
    LocalFree(list);
    return arguments;
}

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    const std::vector<std::wstring> arguments = Arguments();
    if (arguments.empty()) return RunTray(instance);
    if (arguments.size() == 1 && arguments[0] == L"--diag") return RunDiag(DiagOutput::Dialog);
    if (arguments.size() == 2 && arguments[0] == L"--diag" && arguments[1] == L"--console") {
        return RunDiag(DiagOutput::Console);
    }
    Log(L"unknown arguments: %s", GetCommandLineW());
    return 2;
}
