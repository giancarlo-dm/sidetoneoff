// Resource IDs, shared by res/app.rc and the code. #defines only, so rc can read it.
#pragma once

// The app icon (build/app.ico, drawn by make-icon.ps1). Explorer, Task Manager and Installed apps show the
// exe's first icon, so it keeps the lowest ID.
#define IDI_APP 1

// The tray icon's glyph, white on transparent, at every tray size (build/tray.ico, drawn by make-icon.ps1).
// tray_icon.cpp recolors and dims it.
#define IDI_TRAY 2
