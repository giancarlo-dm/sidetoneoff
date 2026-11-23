// The app's recent log lines, kept in memory for the log window (plan, section 5.7; decision M15). Portable: no
// Windows headers, so the unit tests, and a Mac or Linux port, can use it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

class LogHistory {
public:
    explicit LogHistory(std::size_t capacity);

    LogHistory(const LogHistory&) = delete;
    LogHistory& operator=(const LogHistory&) = delete;

    // Adds a line. Past the capacity, the oldest line is dropped. Safe to call from any thread.
    void Add(std::wstring line);

    // The lines added after `cursor`, oldest first, each followed by "\r\n" (a Windows text box's line break),
    // and moves `cursor` past them. Lines are numbered from 1, and the cursor is the number of the last line
    // read, so 0 means "from the start": every line still kept. Safe to call from any thread.
    std::wstring TextSince(std::uint64_t& cursor) const;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;  // `mutable`: TextSince, a const function, still locks it
    std::deque<std::wstring> lines_;
    std::uint64_t added_ = 0;  // lines ever added; lines_ holds the last lines_.size() of them
};
