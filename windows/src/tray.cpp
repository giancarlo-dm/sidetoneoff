#include "tray.h"

#include <shellapi.h>
#include <strsafe.h>

#include <iterator>

#include "log.h"
#include "strings.h"
#include "win32.h"

namespace {

constexpr UINT kIconId = 1;  // identifies our one icon, together with the window

// The fields every Shell_NotifyIconW call needs to name our icon.
NOTIFYICONDATAW IconData(HWND window) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = kIconId;
    return data;
}

// "Sidetone Off — <status>". NIF_SHOWTIP: with NOTIFYICON_VERSION_4 the standard tooltip only shows with it.
void SetTooltip(NOTIFYICONDATAW& data, HeadsetStatus status) {
    data.uFlags |= NIF_TIP | NIF_SHOWTIP;
    StringCchPrintfW(data.szTip, std::size(data.szTip), L"%s%s%s", ui::kAppName, ui::kTooltipSeparator,
                     StatusText(status));
}

}  // namespace

const wchar_t* StatusText(HeadsetStatus status) {
    switch (status) {
    case HeadsetStatus::Disconnected: return ui::kStatusDisconnected;
    case HeadsetStatus::Applying: return ui::kStatusApplying;
    case HeadsetStatus::SidetoneOff: return ui::kStatusSidetoneOff;
    case HeadsetStatus::Failed: return ui::kStatusFailed;
    }
    return L"";
}

Tray::Tray(HWND window, UINT callbackMessage)
    : window_(window), callbackMessage_(callbackMessage), icon_(LoadIconW(nullptr, IDI_APPLICATION)) {
    Add();
}

Tray::~Tray() {
    NOTIFYICONDATAW data = IconData(window_);
    Shell_NotifyIconW(NIM_DELETE, &data);
}

void Tray::Add() {
    NOTIFYICONDATAW data = IconData(window_);
    Shell_NotifyIconW(NIM_DELETE, &data);  // in case it is still there (e.g. TaskbarCreated after a DPI change)

    data.uFlags = NIF_MESSAGE | NIF_ICON;
    data.uCallbackMessage = callbackMessage_;
    data.hIcon = icon_;
    SetTooltip(data, status_);
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        // Normal at sign-in if Explorer isn't up yet; TaskbarCreated will bring us back here.
        Log(L"can't add the tray icon yet (error %lu)", GetLastError());
        return;
    }
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    Log(L"tray icon added");
}

void Tray::Show(HeadsetStatus status) {
    const bool changed = status != status_;
    status_ = status;
    NOTIFYICONDATAW data = IconData(window_);
    SetTooltip(data, status);
    Shell_NotifyIconW(NIM_MODIFY, &data);

    // An open Win32 menu can't be resized, and the status line changes length, so close it and let
    // PopUpMenu() open it again in the same place.
    if (menuOpen_ && changed) {
        reopenMenu_ = true;
        EndMenu();
    }
}

void Tray::CloseMenu() {
    reopenMenu_ = false;
    if (menuOpen_) EndMenu();
}

MenuCommand Tray::PopUpMenu(POINT anchor) {
    MenuCommand command = MenuCommand::None;
    do {
        reopenMenu_ = false;
        menuOpen_ = true;
        command = TrackMenuOnce(anchor);
        menuOpen_ = false;
    } while (command == MenuCommand::None && reopenMenu_);
    // Without the WM_NULL after (and the foreground window before), the menu doesn't close when the user
    // clicks elsewhere: a documented TrackPopupMenu quirk.
    PostMessageW(window_, WM_NULL, 0, 0);
    return command;
}

MenuCommand Tray::TrackMenuOnce(POINT anchor) {
    const UniqueMenu menu{CreatePopupMenu()};
    if (!menu) return MenuCommand::None;

    const bool connected = status_ != HeadsetStatus::Disconnected;
    const bool canSetAgain = status_ == HeadsetStatus::SidetoneOff || status_ == HeadsetStatus::Failed;
    const auto item = [&menu](MenuCommand command, const wchar_t* text, bool enabled) {
        AppendMenuW(menu.get(), MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), static_cast<UINT_PTR>(command), text);
    };
    item(MenuCommand::Status, StatusText(status_), connected);
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    item(MenuCommand::SetAgain, ui::kMenuSetAgain, canSetAgain);
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    item(MenuCommand::Quit, ui::kMenuQuit, true);

    SetForegroundWindow(window_);
    const UINT alignment = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const auto command = static_cast<UINT>(TrackPopupMenuEx(
        menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | alignment, anchor.x, anchor.y,
        window_, nullptr));
    return static_cast<MenuCommand>(command);
}
