// The log window: Tray > Show log. The app's history since it started, in a scrollable, read-only text box
// that follows new lines as they come (plan, section 5.7).
#pragma once

#include <windows.h>

#include <cstdint>

#include "win32.h"

class LogWindow {
public:
    explicit LogWindow(HINSTANCE instance);
    ~LogWindow();  // closes the window if it is open

    // Not copyable or movable: the window holds a pointer to this object.
    LogWindow(const LogWindow&) = delete;
    LogWindow& operator=(const LogWindow&) = delete;

    // Opens the window with the history so far, or brings it to the front if it is open already.
    void Show();

private:
    // Windows calls these static functions; they forward to this object (stored with the window).
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TextBoxProc(HWND textBox, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                        DWORD_PTR self);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool Create();
    void FillFromHistory();    // the whole history, scrolled to the end
    void AppendNewLines();     // the lines since the last ones shown
    void UseDpi(UINT dpi);     // the font and margins for this DPI

    HINSTANCE instance_;
    HWND window_ = nullptr;    // while open
    HWND textBox_ = nullptr;   // the EDIT control filling the window
    UniqueFont font_;
    std::uint64_t cursor_ = 0;      // the last history line in the text box
    std::uint64_t linesShown_ = 0;  // lines in the text box
};
