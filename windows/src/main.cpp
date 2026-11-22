// Sidetone Off for Windows: turns off the Logitech G635's sidetone whenever the headset is connected.
// Lives in the system tray and starts at sign-in.

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "app.h"
#include "diag.h"
#include "log.h"
#include "strings.h"
#include "version.h"
#include "win32.h"

namespace {

// The tray app (no arguments): one instance only, then the App until Quit.
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

    Log(L"started, version %hs", SIDETONE_VERSION_STRING);
    const std::unique_ptr<App> app = App::Create(instance);
    if (!app) return 1;
    const int exitCode = app->Run();
    Log(L"exiting");
    return exitCode;
}  // the App, then the mutex, are released here

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
