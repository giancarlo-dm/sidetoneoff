#include "log_window.h"

#include <commctrl.h>  // SetWindowSubclass

#include <algorithm>
#include <string>
#include <utility>

#include "../res/resource.h"
#include "log.h"
#include "strings.h"

namespace {

constexpr UINT kNewLinesMessage = WM_APP + 1;  // posted by Log() through WatchLog: new lines in the history
constexpr int kWidth = 820;                     // the window's size at 100% scale; scaled for its monitor
constexpr int kHeight = 460;
constexpr int kFontPoints = 10;
constexpr int kMarginPixels = 6;  // inside the text box, at 100% scale
// The window can stay open for days. When its text box holds this many lines, it is refilled with the history,
// which keeps the last 500.
constexpr std::uint64_t kRefillAt = 1000;

std::uint64_t CountLines(const std::wstring& text) {
    return static_cast<std::uint64_t>(std::count(text.begin(), text.end(), L'\n'));
}

bool RegisterWindowClass(HINSTANCE instance, WNDPROC windowProc) {
    const auto appIcon = [instance](int sizeMetric) {
        const int size = GetSystemMetrics(sizeMetric);
        return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, size, size, LR_SHARED));
    };
    const WNDCLASSEXW windowClass{
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = windowProc,
        .hInstance = instance,
        .hIcon = appIcon(SM_CXICON),
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)),  // a system color, by number
        .lpszClassName = ids::kLogWindowClass,
        .hIconSm = appIcon(SM_CXSMICON),
    };
    return RegisterClassExW(&windowClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

}  // namespace

LogWindow::LogWindow(HINSTANCE instance) : instance_(instance) {}

LogWindow::~LogWindow() {
    if (window_) DestroyWindow(window_);
}

void LogWindow::Show() {
    if (window_) {
        if (IsIconic(window_)) ShowWindow(window_, SW_RESTORE);
        SetForegroundWindow(window_);
        return;
    }
    if (!Create()) return;
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    SetFocus(textBox_);
    FillFromHistory();  // once shown, so "scroll to the end" knows the real size
}

bool LogWindow::Create() {
    if (!RegisterWindowClass(instance_, WindowProc)) {
        Log(L"can't register the log window class (error %lu)", GetLastError());
        return false;
    }
    // Hidden at first: it is sized for its monitor's DPI once it exists, then shown. The last argument hands this
    // object to WindowProc (WM_NCCREATE), which links the window to it and sets window_.
    if (!CreateWindowExW(0, ids::kLogWindowClass, ui::kLogTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         kWidth, kHeight, nullptr, nullptr, instance_, this)) {
        Log(L"can't create the log window (error %lu)", GetLastError());
        return false;
    }
    const UINT dpi = GetDpiForWindow(window_);
    SetWindowPos(window_, nullptr, 0, 0, MulDiv(kWidth, static_cast<int>(dpi), 96),
                 MulDiv(kHeight, static_cast<int>(dpi), 96), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Read-only and multiline; without ES_AUTOHSCROLL it wraps long lines; ES_NOHIDESEL keeps a selection visible
    // when the window loses focus.
    textBox_ = CreateWindowExW(0, L"EDIT", nullptr,
                               WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL, 0, 0,
                               0, 0, window_, nullptr, instance_, nullptr);
    if (!textBox_) {
        Log(L"can't create the log window's text box (error %lu)", GetLastError());
        DestroyWindow(window_);
        return false;
    }
    SetWindowSubclass(textBox_, TextBoxProc, 0, reinterpret_cast<DWORD_PTR>(this));
    SendMessageW(textBox_, EM_SETLIMITTEXT, 0, 0);  // no 32,000-character cap
    UseDpi(dpi);
    RECT client{};
    GetClientRect(window_, &client);
    MoveWindow(textBox_, 0, 0, client.right, client.bottom, FALSE);

    // Before the history is read: a line logged in between is then both read and announced, and the second read
    // finds nothing new, rather than the line being missed.
    WatchLog(window_, kNewLinesMessage);
    return true;
}

void LogWindow::FillFromHistory() {
    cursor_ = 0;
    const std::wstring text = LogLines().TextSince(cursor_);
    SetWindowTextW(textBox_, text.c_str());
    linesShown_ = CountLines(text);
    const int length = GetWindowTextLengthW(textBox_);
    SendMessageW(textBox_, EM_SETSEL, length, length);  // the caret at the end
    SendMessageW(textBox_, WM_VSCROLL, SB_BOTTOM, 0);
}

void LogWindow::AppendNewLines() {
    if (!textBox_) return;
    const std::wstring text = LogLines().TextSince(cursor_);
    if (text.empty()) return;
    linesShown_ += CountLines(text);
    if (linesShown_ >= kRefillAt) {
        FillFromHistory();
        return;
    }

    // Keep the reader's place: follow the new lines only if the end was in view, and keep any selection.
    SCROLLINFO scroll{.cbSize = sizeof(SCROLLINFO), .fMask = SIF_ALL};
    GetScrollInfo(textBox_, SB_VERT, &scroll);
    const bool endInView = scroll.nPos + static_cast<int>(scroll.nPage) > scroll.nMax;
    DWORD selectionStart = 0;
    DWORD selectionEnd = 0;
    SendMessageW(textBox_, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStart), reinterpret_cast<LPARAM>(&selectionEnd));
    const LRESULT firstVisibleLine = SendMessageW(textBox_, EM_GETFIRSTVISIBLELINE, 0, 0);

    const int length = GetWindowTextLengthW(textBox_);
    SendMessageW(textBox_, EM_SETSEL, length, length);
    SendMessageW(textBox_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));  // also moves the view

    if (selectionStart != selectionEnd) SendMessageW(textBox_, EM_SETSEL, selectionStart, selectionEnd);
    if (endInView) {
        SendMessageW(textBox_, WM_VSCROLL, SB_BOTTOM, 0);
    } else {
        const LRESULT nowFirstVisible = SendMessageW(textBox_, EM_GETFIRSTVISIBLELINE, 0, 0);
        SendMessageW(textBox_, EM_LINESCROLL, 0, firstVisibleLine - nowFirstVisible);
    }
}

void LogWindow::UseDpi(UINT dpi) {
    // Consolas at 10 points for this DPI: a point is 1/72 inch, and a negative height asks for the character height.
    UniqueFont font{CreateFontW(-MulDiv(kFontPoints, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                FIXED_PITCH | FF_MODERN, L"Consolas")};
    SendMessageW(textBox_, WM_SETFONT, reinterpret_cast<WPARAM>(font.get()), TRUE);
    font_ = std::move(font);  // the old font is deleted only now, once the text box has stopped using it
    const int margin = MulDiv(kMarginPixels, static_cast<int>(dpi), 96);
    SendMessageW(textBox_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
}

LRESULT CALLBACK LogWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {  // the first message: store the LogWindow passed to CreateWindowExW
        auto* self = static_cast<LogWindow*>(reinterpret_cast<const CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<LogWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT LogWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kNewLinesMessage:
        AppendNewLines();
        return 0;
    case WM_SIZE:
        if (textBox_) MoveWindow(textBox_, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        return 0;
    case WM_SETFOCUS:
        if (textBox_) SetFocus(textBox_);
        return 0;
    case WM_CTLCOLORSTATIC:
        // A read-only text box asks for a label's colors, which are gray. Keep it a white text box.
        if (reinterpret_cast<HWND>(lParam) == textBox_) {
            const auto dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(dc, GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        break;
    case WM_DPICHANGED: {
        // Moved to a monitor with another scale: take the size Windows suggests, and a font for the new DPI.
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        UseDpi(LOWORD(wParam));
        return 0;
    }
    case WM_DESTROY:
        WatchLog(nullptr, 0);
        return 0;
    case WM_NCDESTROY: {
        // The window's last message. Its text box is gone already, so the font can go too.
        const HWND window = window_;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);  // no more messages for this object
        window_ = nullptr;
        textBox_ = nullptr;
        font_.reset();
        cursor_ = 0;
        linesShown_ = 0;
        return DefWindowProcW(window, message, wParam, lParam);
    }
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

// The text box's messages pass through here first (a "subclass"): it adds Esc to close and Ctrl+A to select all.
LRESULT CALLBACK LogWindow::TextBoxProc(HWND textBox, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR,
                                        DWORD_PTR self) {
    switch (message) {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostMessageW(reinterpret_cast<LogWindow*>(self)->window_, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_CHAR:
        if (wParam == 1) {  // Ctrl+A, which a plain text box doesn't handle by itself
            SendMessageW(textBox, EM_SETSEL, 0, -1);
            return 0;
        }
        if (wParam == VK_ESCAPE) return 0;  // handled at WM_KEYDOWN; this keeps the text box from beeping
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(textBox, TextBoxProc, 0);
        break;
    }
    return DefSubclassProc(textBox, message, wParam, lParam);
}
