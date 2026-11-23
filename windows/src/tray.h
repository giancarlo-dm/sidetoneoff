// The tray icon, its tooltip and its menu (plan, section 5.4; spec, "Menu").
#pragma once

#include <windows.h>
#include <shellapi.h>  // NOTIFYICONDATAW

#include "tray_icon.h"
#include "win32.h"

// The headset's state as the user sees it (spec's state table).
enum class HeadsetStatus {
    Disconnected,
    Applying,     // turning sidetone off
    SidetoneOff,
    Failed,       // couldn't turn it off
};

// The status line for a state, e.g. "Logitech G635 connected — sidetone off".
const wchar_t* StatusText(HeadsetStatus status);

// The menu items. 0 means the menu was dismissed.
enum class MenuCommand : UINT {
    None = 0,
    Status,    // the status line: always disabled, so it can't be picked
    SetAgain,  // "Set sidetone off again"
    ShowLog,
    Uninstall,
    Quit,
};

class Tray {
public:
    // Adds the icon. Clicks on it are sent to `window` as `callbackMessage` (NOTIFYICON_VERSION_4 format).
    Tray(HWND window, UINT callbackMessage);
    ~Tray();  // removes the icon

    Tray(const Tray&) = delete;
    Tray& operator=(const Tray&) = delete;

    // Adds the icon again. Needed after Explorer restarts (the TaskbarCreated message), which drops every icon.
    // Explorer also sends TaskbarCreated when the taskbar's DPI changes, so the icon is redrawn if needed.
    void Add();

    // Redraws the icon if the taskbar's theme or DPI changed since it was drawn; otherwise does nothing.
    // Call it when Windows says a setting, the DPI or the display changed.
    void UpdateIcon();

    // Updates the tooltip and the icon (dimmed while disconnected) to `status`, which the menu also shows.
    // If the menu is open, it is reopened in place with the new status, so it never shows a stale one.
    void Show(HeadsetStatus status);

    // Shows a notification from the icon, with the app icon: a balloon on Windows 10, a toast on Windows 11.
    // False if Windows didn't take it (e.g. the icon isn't in the tray yet).
    bool ShowNotice(const wchar_t* title, const wchar_t* text);

    // Shows the menu at `anchor` (screen coordinates) and returns the item picked. Returns only when the
    // menu closes; messages keep being handled meanwhile, so Show() can run while it is open.
    MenuCommand PopUpMenu(POINT anchor);

    bool IsMenuOpen() const { return menuOpen_; }

    // Closes the menu if it is open; PopUpMenu() then returns MenuCommand::None.
    void CloseMenu();

private:
    MenuCommand TrackMenuOnce(POINT anchor);
    bool RedrawIconsIfStale();  // true if it drew them again
    HICON CurrentIcon() const;  // the icon for status_
    NOTIFYICONDATAW CurrentData() const;  // the icon and tooltip for status_, for NIM_ADD and NIM_MODIFY

    HWND window_;
    UINT callbackMessage_;
    HeadsetStatus status_ = HeadsetStatus::Disconnected;
    TaskbarStyle iconStyle_;  // what icon_ and dimmedIcon_ were drawn for
    UniqueIcon icon_;
    UniqueIcon dimmedIcon_;  // shown while disconnected
    bool menuOpen_ = false;
    bool reopenMenu_ = false;  // set when the status changes while the menu is open
};
