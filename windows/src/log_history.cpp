#include "log_history.h"

#include <utility>

LogHistory::LogHistory(std::size_t capacity) : capacity_(capacity) {}

void LogHistory::Add(std::wstring line) {
    const std::lock_guard lock(mutex_);  // holds the mutex until the end of this scope, like Java's synchronized
    lines_.push_back(std::move(line));   // std::move: hand the string over instead of copying it
    if (lines_.size() > capacity_) lines_.pop_front();
    ++added_;
}

std::wstring LogHistory::TextSince(std::uint64_t& cursor) const {
    const std::lock_guard lock(mutex_);
    const std::uint64_t dropped = added_ - lines_.size();  // lines numbered 1..dropped are gone
    // Skip the kept lines the reader has seen already: those numbered up to `cursor`.
    std::size_t first = 0;
    if (cursor > dropped) first = static_cast<std::size_t>(cursor - dropped);
    std::wstring text;
    for (std::size_t i = first; i < lines_.size(); ++i) {
        text += lines_[i];
        text += L"\r\n";
    }
    cursor = added_;
    return text;
}
