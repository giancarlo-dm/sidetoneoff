// Sidetone Off for Windows: turns off the Logitech G635's sidetone whenever the headset is connected.
// Installs itself, lives in the system tray and starts at sign-in.

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "app.h"
#include "diag.h"
#include "install.h"
#include "log.h"
#include "strings.h"
#include "version.h"
#include "win32.h"

namespace {

// The tray app (run from the install folder): one instance only, then the App until Quit or Uninstall.
// `showNotice`: false for the start at sign-in (--autostart).
int RunTray(HINSTANCE instance, bool showNotice) {
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
    std::unique_ptr<App> app = App::Create(instance, showNotice);
    if (!app) return 1;
    const int exitCode = app->Run();
    const bool uninstalled = app->Uninstalled();
    app.reset();  // the worker has stopped and the tray icon is gone: only the exit is left

    if (uninstalled) {
        const auto removed = RemoveInstalledFiles();
        if (!removed) Log(L"uninstall failed: %s", DescribeError(removed.error()).c_str());
    }
    Log(L"exiting");
    return exitCode;
}  // the mutex is released here

// Install or update: with no arguments from outside the install folder, after asking (decision M3), with
// errors shown in a dialog; with --install (build.ps1 -Install), straight away, with errors only logged.
int RunInstall(bool ask) {
    if (ask && !ConfirmInstall(IsInstalled())) {
        Log(L"install cancelled");
        return 0;
    }
    const auto installed = Install();
    if (!installed) {
        Log(L"install failed: %s", DescribeError(installed.error()).c_str());
        if (ask) ShowError(ui::kInstallFailed, installed.error());
        return 1;
    }
    return 0;
}

// --uninstall, from Settings > Apps: no confirmation, since Windows asked already (decision M4).
int RunUninstall() {
    const auto uninstalled = Uninstall();
    if (!uninstalled) {
        Log(L"uninstall failed: %s", DescribeError(uninstalled.error()).c_str());
        ShowError(ui::kUninstallFailed, uninstalled.error());
        return 1;
    }
    return 0;
}

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
    // Plan, section 5.3, "Entry points".
    const std::vector<std::wstring> arguments = Arguments();
    if (arguments.empty()) return IsInstalledCopy() ? RunTray(instance, true) : RunInstall(true);
    if (arguments.size() == 1 && arguments[0] == ids::kAutostartArgument && IsInstalledCopy()) {
        return RunTray(instance, false);  // the Run value, at sign-in
    }
    if (arguments.size() == 1 && arguments[0] == L"--install") return RunInstall(false);
    if (arguments.size() == 1 && arguments[0] == L"--uninstall") return RunUninstall();
    if (arguments.size() == 1 && arguments[0] == L"--diag") return RunDiag(DiagOutput::Dialog);
    if (arguments.size() == 2 && arguments[0] == L"--diag" && arguments[1] == L"--console") {
        return RunDiag(DiagOutput::Console);
    }
    Log(L"unknown arguments: %s", GetCommandLineW());
    return 2;
}
