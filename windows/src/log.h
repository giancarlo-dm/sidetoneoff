// Debug log: "Sidetone Off: ..." lines via OutputDebugString, readable with DebugView or any
// debugger. No log files.
#pragma once

#include <windows.h>
#include <strsafe.h>

#include <cstdarg>
#include <iterator>

#include "strings.h"

inline void Log(_Printf_format_string_ const wchar_t* format, ...) {
    wchar_t line[1024];
    wchar_t* end = nullptr;
    size_t left = 0;
    StringCchCopyExW(line, std::size(line), ids::kLogPrefix, &end, &left, 0);

    va_list args;
    va_start(args, format);
    StringCchVPrintfExW(end, left, &end, &left, 0, format, args);  // a truncated line is fine
    va_end(args);

    StringCchCopyW(end, left, L"\n");
    OutputDebugStringW(line);
}
