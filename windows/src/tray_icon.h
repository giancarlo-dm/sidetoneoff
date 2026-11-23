// The tray icon's picture: the headphones glyph, colored for the taskbar's theme and sized for its DPI
// (plan, section 5.4).
#pragma once

#include <windows.h>

#include "win32.h"

// What the tray icon has to match.
struct TaskbarStyle {
    int iconSize = 16;   // pixels: the small-icon size at the taskbar's DPI (16 at 100%, 24 at 150%)
    bool light = false;  // light taskbar: black glyph; dark taskbar: white glyph
};

// Reads how the taskbar looks now: its DPI and Windows' light/dark mode setting.
TaskbarStyle CurrentTaskbarStyle();

// The headphones glyph (decision M12, drawn by make-icon.ps1) as an icon of `style.iconSize` pixels, in the
// taskbar's color, at 40% opacity when `dimmed`. Empty (logged) on failure.
UniqueIcon RenderTrayIcon(const TaskbarStyle& style, bool dimmed);
