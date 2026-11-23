// The app's log. Each line goes to OutputDebugString as "SidetoneOff: ...", readable with DebugView or any
// debugger, and, with its date and time, into this process's history, which the log window shows (plan,
// section 5.7). No log files.
#pragma once

#include <windows.h>

#include "log_history.h"

// printf-style: Log(L"opened %s (error %lu)", path, error).
void Log(_Printf_format_string_ const wchar_t* format, ...);

// This process's history: the last 500 lines, each "2026-11-23 14:02:12  <message>".
LogHistory& LogLines();

// After each new line, posts `message` to `window`, which then reads the new lines from LogLines(). One window at
// a time; nullptr stops it. Lines come from any thread, so this is how they reach the window's thread.
void WatchLog(HWND window, UINT message);
