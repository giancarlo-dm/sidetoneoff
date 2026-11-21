// Every user-visible string, plus the names other processes rely on, in one place
// (so the Mac backport and a Linux port can reuse the exact wording).
#pragma once

namespace ui {

inline constexpr wchar_t kAppName[] = L"Sidetone Off";

}  // namespace ui

namespace ids {

// Install and uninstall find the running instance by this window class.
inline constexpr wchar_t kWindowClass[] = L"SidetoneOff.Window";

// Single-instance mutex: a second launch exits silently.
inline constexpr wchar_t kMutex[] = L"Local\\SidetoneOff.SingleInstance";

// Prefix of every OutputDebugString line, same as the Mac app's NSLog messages.
inline constexpr wchar_t kLogPrefix[] = L"Sidetone Off: ";

}  // namespace ids
