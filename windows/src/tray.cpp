#include "tray.h"

#include <commctrl.h>  // LoadIconMetric
#include <shellapi.h>
#include <strsafe.h>

#include <iterator>

#include "../res/resource.h"
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

Tray::Tray(HWND window, UINT callbackMessage) : window_(window), callbackMessage_(callbackMessage) {
    Add();
}

Tray::~Tray() {
    NOTIFYICONDATAW data = IconData(window_);
    Shell_NotifyIconW(NIM_DELETE, &data);
}

void Tray::Add() {
    RedrawIconsIfStale();

    NOTIFYICONDATAW gone = IconData(window_);
    Shell_NotifyIconW(NIM_DELETE, &gone);  // in case it is still there (e.g. TaskbarCreated after a DPI change)

    NOTIFYICONDATAW data = CurrentData();
    data.uFlags |= NIF_MESSAGE;
    data.uCallbackMessage = callbackMessage_;
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        // Normal at sign-in if Explorer isn't up yet; TaskbarCreated will bring us back here.
        Log(L"can't add the tray icon yet (error %lu)", GetLastError());
        return;
    }
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    // Version 4 hides the standard tooltip unless NIF_SHOWTIP comes with the data, and the one sent with NIM_ADD,
    // before the version was set, doesn't count: without this, the tooltip only appeared after the first change.
    data = CurrentData();
    Shell_NotifyIconW(NIM_MODIFY, &data);
    Log(L"tray icon added");
}

NOTIFYICONDATAW Tray::CurrentData() const {
    NOTIFYICONDATAW data = IconData(window_);
    // NIF_SHOWTIP goes with every call: with NOTIFYICON_VERSION_4, the standard tooltip only shows with it.
    data.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.hIcon = CurrentIcon();
    // Two lines: "Sidetone Off", then the status (decision M13).
    StringCchPrintfW(data.szTip, std::size(data.szTip), L"%s\n%s", ui::kAppName, StatusText(status_));
    return data;
}

void Tray::UpdateIcon() {
    if (!RedrawIconsIfStale()) return;
    NOTIFYICONDATAW data = CurrentData();
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

bool Tray::RedrawIconsIfStale() {
    const TaskbarStyle style = CurrentTaskbarStyle();
    if (icon_ && dimmedIcon_ && style.iconSize == iconStyle_.iconSize && style.light == iconStyle_.light) {
        return false;
    }
    iconStyle_ = style;
    icon_ = RenderTrayIcon(style, false);
    dimmedIcon_ = RenderTrayIcon(style, true);
    Log(L"tray icon drawn: %d px, %s taskbar", style.iconSize, style.light ? L"light" : L"dark");
    return true;
}

HICON Tray::CurrentIcon() const {
    const UniqueIcon& icon = status_ == HeadsetStatus::Disconnected ? dimmedIcon_ : icon_;
    // If drawing failed (logged), a stock icon keeps the tray icon there, and its menu reachable.
    return icon ? icon.get() : LoadIconW(nullptr, IDI_APPLICATION);
}

void Tray::Show(HeadsetStatus status) {
    const bool changed = status != status_;
    status_ = status;
    NOTIFYICONDATAW data = CurrentData();
    Shell_NotifyIconW(NIM_MODIFY, &data);

    // An open Win32 menu can't be resized, and the status line changes length, so close it and let
    // PopUpMenu() open it again in the same place.
    if (menuOpen_ && changed) {
        reopenMenu_ = true;
        EndMenu();
    }
}

bool Tray::ShowNotice(const wchar_t* title, const wchar_t* text) {
    NOTIFYICONDATAW data = CurrentData();  // with the tooltip, so this call doesn't hide it
    data.uFlags |= NIF_INFO;
    StringCchCopyW(data.szInfoTitle, std::size(data.szInfoTitle), title);
    StringCchCopyW(data.szInfo, std::size(data.szInfo), text);
    // The app icon at the large-icon size for this DPI. The tray glyph would be white on white in a light toast.
    HICON appIcon = nullptr;
    if (FAILED(LoadIconMetric(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), LIM_LARGE, &appIcon))) {
        appIcon = nullptr;
    }
    const UniqueIcon ownedIcon{appIcon};  // Windows keeps its own copy
    data.hBalloonIcon = appIcon;
    data.dwInfoFlags = appIcon ? NIIF_USER | NIIF_LARGE_ICON : NIIF_INFO;
    return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
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

    const bool canSetAgain = status_ == HeadsetStatus::SidetoneOff || status_ == HeadsetStatus::Failed;
    const auto item = [&menu](MenuCommand command, const wchar_t* text, bool enabled) {
        AppendMenuW(menu.get(), MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), static_cast<UINT_PTR>(command), text);
    };
    item(MenuCommand::Status, StatusText(status_), false);  // a status, not a command: never clickable, as on the Mac
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    item(MenuCommand::SetAgain, ui::kMenuSetAgain, canSetAgain);
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    item(MenuCommand::ShowLog, ui::kMenuShowLog, true);
    item(MenuCommand::Uninstall, ui::kMenuUninstall, true);
    item(MenuCommand::Quit, ui::kMenuQuit, true);

    SetForegroundWindow(window_);
    const UINT alignment = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const auto command = static_cast<UINT>(TrackPopupMenuEx(
        menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | alignment, anchor.x, anchor.y,
        window_, nullptr));
    return static_cast<MenuCommand>(command);
}
