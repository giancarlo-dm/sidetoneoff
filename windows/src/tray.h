// The tray icon, its tooltip and its menu (plan, section 5.4; spec, "Menu").
#pragma once

#include <windows.h>

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
    Status,    // the status line: enabled while connected, but does nothing
    SetAgain,  // "Set sidetone off again"
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
    void Add();

    // Updates the tooltip (and, from Phase 4, the icon) to `status`, which the menu also shows. If the menu
    // is open, it is reopened in place with the new status, so it never shows a stale one.
    void Show(HeadsetStatus status);

    // Shows the menu at `anchor` (screen coordinates) and returns the item picked. Returns only when the
    // menu closes; messages keep being handled meanwhile, so Show() can run while it is open.
    MenuCommand PopUpMenu(POINT anchor);

    bool IsMenuOpen() const { return menuOpen_; }

    // Closes the menu if it is open; PopUpMenu() then returns MenuCommand::None.
    void CloseMenu();

private:
    MenuCommand TrackMenuOnce(POINT anchor);

    HWND window_;
    UINT callbackMessage_;
    HeadsetStatus status_ = HeadsetStatus::Disconnected;
    HICON icon_;  // Phase 3 placeholder: a shared system icon, not owned
    bool menuOpen_ = false;
    bool reopenMenu_ = false;  // set when the status changes while the menu is open
};
