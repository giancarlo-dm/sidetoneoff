// Install, update and uninstall, with no installer: the exe installs itself (plan, section 5.3). Free functions,
// since nothing here outlives a call.
#pragma once

#include <windows.h>

#include <expected>
#include <string>

// A failed step, and Windows' error code for it (a Win32 error, or an HRESULT from COM and the shell).
struct InstallError {
    const wchar_t* step;  // what we were doing, to follow "Couldn't ": e.g. L"copy the app into its folder"
    DWORD code;
};

// "Couldn't copy the app into its folder: Access is denied. (error 5)"
std::wstring DescribeError(const InstallError& error);

// True if this process runs the installed exe: the same file, not a copy of it.
bool IsInstalledCopy();

// True if the installed exe exists.
bool IsInstalled();

// Install steps: closes the running instance, copies this exe into the install folder (unless it is the
// installed one), adds it to startup, the Start menu and Installed apps, and starts it.
std::expected<void, InstallError> Install();

// Uninstall steps, for --uninstall: closes the running instance, then Unregister() and RemoveInstalledFiles().
std::expected<void, InstallError> Uninstall();

// Removes the Run value, the Installed apps entry, the Start menu shortcut and the app's own registry key.
// Anything already gone is fine. The files stay: see RemoveInstalledFiles().
std::expected<void, InstallError> Unregister();

// Deletes the installed exe and its folder (the folder only if it is empty). If this process is that exe, it
// can't delete itself, so a hidden cmd.exe deletes both about 2 s later: call this just before exiting.
std::expected<void, InstallError> RemoveInstalledFiles();

// The dialog for an exe started with no arguments from outside the install folder (decision M3):
// "Install SidetoneOff?", or "Update SidetoneOff to x.y.z?" when `update`. True if the user said yes.
bool ConfirmInstall(bool update);

// Tray > Uninstall…'s dialog (spec). Cancel is the default, so Enter doesn't uninstall by accident. True if the
// user chose Uninstall. While it is open, `*openDialog` is its window, so the app can close it.
bool ConfirmUninstall(HWND* openDialog);

// An error dialog: `title` (e.g. "Couldn't install SidetoneOff") over DescribeError(error). `openDialog` as above.
void ShowError(const wchar_t* title, const InstallError& error, HWND* openDialog = nullptr);
