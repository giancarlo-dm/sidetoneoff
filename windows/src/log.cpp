#include "log.h"

#include <strsafe.h>

#include <cstdarg>
#include <iterator>
#include <mutex>

#include "strings.h"

namespace {

constexpr std::size_t kHistoryLines = 500;  // decision M15

// Who wants to hear about new lines: the log window, while it is open.
std::mutex watcherMutex;
HWND watcherWindow = nullptr;
UINT watcherMessage = 0;

}  // namespace

LogHistory& LogLines() {
    static LogHistory history(kHistoryLines);  // made on first use; C++ makes that safe across threads
    return history;
}

void WatchLog(HWND window, UINT message) {
    const std::lock_guard lock(watcherMutex);
    watcherWindow = window;
    watcherMessage = message;
}

void Log(_Printf_format_string_ const wchar_t* format, ...) {
    wchar_t message[1024];
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, std::size(message), format, args);  // a truncated line is fine
    va_end(args);

    wchar_t line[1100];
    StringCchPrintfW(line, std::size(line), L"%s%s\n", ids::kLogPrefix, message);
    OutputDebugStringW(line);

    SYSTEMTIME now{};
    GetLocalTime(&now);
    StringCchPrintfW(line, std::size(line), L"%04u-%02u-%02u %02u:%02u:%02u  %s", now.wYear, now.wMonth, now.wDay,
                     now.wHour, now.wMinute, now.wSecond, message);
    LogLines().Add(line);

    const std::lock_guard lock(watcherMutex);
    if (watcherWindow) PostMessageW(watcherWindow, watcherMessage, 0, 0);
}
