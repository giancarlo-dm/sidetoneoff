// Every string of the tray UI, plus the names other processes rely on, in one place
// (so the Mac backport and a Linux port can reuse the exact wording). The technical --diag report
// keeps its own text in diag.cpp, and install.cpp the Windows-only names of its steps.
#pragma once

#include "version.h"

namespace ui {

inline constexpr wchar_t kAppName[] = L"SidetoneOff";
inline constexpr wchar_t kPublisher[] = L"Giancarlo Dalle Mole";  // Installed apps (decision M5); also in app.rc

// Status line, first item of the tray menu. The tooltip is "SidetoneOff" with the status on a second line.
// When sidetone is off, the status is just "connected": that is the app's normal state (decision M13).
inline constexpr wchar_t kStatusDisconnected[] = L"Logitech G635 disconnected";
inline constexpr wchar_t kStatusApplying[] = L"Logitech G635 connected — turning off sidetone…";
inline constexpr wchar_t kStatusSidetoneOff[] = L"Logitech G635 connected";
inline constexpr wchar_t kStatusFailed[] = L"Logitech G635 connected — couldn't turn off sidetone";

// Tray menu commands.
inline constexpr wchar_t kMenuSetAgain[] = L"Set sidetone off again";
inline constexpr wchar_t kMenuShowLog[] = L"Show log";
inline constexpr wchar_t kMenuUninstall[] = L"Uninstall…";
inline constexpr wchar_t kMenuQuit[] = L"Quit";

// The log window's title (Tray > Show log).
inline constexpr wchar_t kLogTitle[] = L"SidetoneOff — Log";

// The notice shown each time you start the app (not at sign-in; decision M14). Windows shows the app's name
// above it.
inline constexpr wchar_t kNoticeTitle[] = L"SidetoneOff is running";
inline constexpr wchar_t kNoticeText[] =
    L"It turns off your G635's sidetone whenever it's connected. Find the headphones icon in the system tray.";

// The dialog shown when the exe is started from outside the install folder (decision M3).
inline constexpr wchar_t kInstallQuestion[] = L"Install SidetoneOff?";
inline constexpr wchar_t kInstallText[] =
    L"It turns off your Logitech G635's sidetone whenever the headset is connected. It starts when you sign in, "
    L"runs in the system tray, and is installed for your account only.";
inline constexpr wchar_t kInstallButton[] = L"Install";
// A wide literal next to the narrow SIDETONE_VERSION_STRING makes one wide string: "Update SidetoneOff to 1.0.0?".
inline constexpr wchar_t kUpdateQuestion[] = L"Update SidetoneOff to " SIDETONE_VERSION_STRING L"?";
inline constexpr wchar_t kUpdateText[] =
    L"This version replaces the installed one. If SidetoneOff is running, it closes and starts again.";
inline constexpr wchar_t kUpdateButton[] = L"Update";

// Tray > Uninstall… (spec, "Uninstall…").
inline constexpr wchar_t kUninstallQuestion[] = L"Uninstall SidetoneOff?";
inline constexpr wchar_t kUninstallText[] =
    L"It will be removed from startup and deleted. Your headset's sidetone won't be turned off automatically "
    L"anymore.";
inline constexpr wchar_t kUninstallButton[] = L"Uninstall";

// Error dialogs. The text below the title says which step failed, and why.
inline constexpr wchar_t kInstallFailed[] = L"Couldn't install SidetoneOff";
inline constexpr wchar_t kUninstallFailed[] = L"Couldn't uninstall SidetoneOff";

// The Start menu shortcut's tooltip.
inline constexpr wchar_t kShortcutDescription[] = L"Turns off the Logitech G635's sidetone whenever it's connected";

}  // namespace ui

namespace ids {

// Install and uninstall find the running instance by this window class.
inline constexpr wchar_t kWindowClass[] = L"SidetoneOff.Window";
inline constexpr wchar_t kLogWindowClass[] = L"SidetoneOff.LogWindow";

// Single-instance mutex: a second launch exits silently.
inline constexpr wchar_t kMutex[] = L"Local\\SidetoneOff.SingleInstance";

// Prefix of every OutputDebugString line, same as the Mac app's NSLog messages.
inline constexpr wchar_t kLogPrefix[] = L"SidetoneOff: ";

// Where the app is installed (plan, section 5.3). The folder is under %LOCALAPPDATA%\Programs
// (FOLDERID_UserProgramFiles); the shortcut is in the Start menu's Programs folder.
inline constexpr wchar_t kInstallFolder[] = L"SidetoneOff";
inline constexpr wchar_t kExeName[] = L"SidetoneOff.exe";
inline constexpr wchar_t kShortcutName[] = L"SidetoneOff.lnk";

// Registry, all under HKEY_CURRENT_USER.
inline constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t kRunValue[] = L"SidetoneOff";
// Where Settings > Apps > Startup keeps a startup app's on/off switch, under the same value name.
inline constexpr wchar_t kStartupApprovedKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
inline constexpr wchar_t kUninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SidetoneOff";
// The app's own settings. Nothing is stored there yet (the next version keeps its RGB colors there), but
// uninstalling deletes it.
inline constexpr wchar_t kAppKey[] = L"Software\\SidetoneOff";

// The argument the Run value passes, so a start at sign-in shows no notice (decision M14).
inline constexpr wchar_t kAutostartArgument[] = L"--autostart";

}  // namespace ids
