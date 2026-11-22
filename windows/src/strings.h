// Every string of the tray UI, plus the names other processes rely on, in one place
// (so the Mac backport and a Linux port can reuse the exact wording). The technical --diag report
// keeps its own text in diag.cpp.
#pragma once

namespace ui {

inline constexpr wchar_t kAppName[] = L"Sidetone Off";

// Status line, first item of the tray menu. The tooltip is "Sidetone Off — <status>".
inline constexpr wchar_t kStatusDisconnected[] = L"Logitech G635 disconnected";
inline constexpr wchar_t kStatusApplying[] = L"Logitech G635 connected — turning off sidetone…";
inline constexpr wchar_t kStatusSidetoneOff[] = L"Logitech G635 connected — sidetone off";
inline constexpr wchar_t kStatusFailed[] = L"Logitech G635 connected — couldn't turn off sidetone";
inline constexpr wchar_t kTooltipSeparator[] = L" — ";

// Tray menu commands.
inline constexpr wchar_t kMenuSetAgain[] = L"Set sidetone off again";
inline constexpr wchar_t kMenuQuit[] = L"Quit";

}  // namespace ui

namespace ids {

// Install and uninstall find the running instance by this window class.
inline constexpr wchar_t kWindowClass[] = L"SidetoneOff.Window";

// Single-instance mutex: a second launch exits silently.
inline constexpr wchar_t kMutex[] = L"Local\\SidetoneOff.SingleInstance";

// Prefix of every OutputDebugString line, same as the Mac app's NSLog messages.
inline constexpr wchar_t kLogPrefix[] = L"Sidetone Off: ";

}  // namespace ids
