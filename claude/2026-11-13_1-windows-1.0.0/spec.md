# Sidetone Off — Windows

**Model:** Opus, extra-high effort, in Claude in the browser.

Windows port of my macOS menu-bar app **Sidetone Off** (reference source in `mac-reference/`).
It turns off the Logitech G635's sidetone whenever the headset is connected. Tray app, starts at
login, shipped as an installer. Replaces Logitech G HUB for this one job (G HUB is uninstalled).
**No external dependencies:** the app talks HID to the headset itself; no `headsetcontrol.exe`.

## How to talk to me
- Portuguese (pt-BR), succinct and direct. Back claims with facts/docs.
- My Windows is in English: give Windows UI paths with English names.
- Prefer commands (winget / PowerShell) over manual downloads.

## Facts
- Logitech VID `0x046D`, G635 PID `0x0A89` (from the working Mac version, `mac-reference/Sidetone Off/main.swift`).
- The Mac version shells out to `headsetcontrol -s 0`. The Windows version sends the same HID packet
  directly (see "Turning sidetone off").
- The HID interface can come up ~1 s after the USB device: wait 1 s, try, up to 5 attempts (Mac does this).
- Detection is event-driven (IOKit matching notification on Mac), never polling. Keep it that way.
- The HID packet has **not** been tested on Windows yet. The first test is live, with the final build, on my
  PC. Make failures easy to diagnose (see Diagnostics) instead of guessing.

## Stack and constraints
- C++20, pure Win32 API. No MFC/ATL/.NET/Qt/WinUI, no hidapi. x64, Unicode, `/SUBSYSTEM:WINDOWS`.
- Static CRT (`/MT`) so no VC++ redistributable is needed. Link only Windows SDK libs
  (`hid.lib`, `cfgmgr32.lib`, `shell32.lib`, `comctl32.lib`, `user32.lib`, `gdi32.lib`, `advapi32.lib` as needed).
- Targets: exe < 300 KB, working set < 5 MB, 0% CPU when idle. No polling, no timers except the retry.
- Manifest: `asInvoker`, PerMonitorV2 DPI awareness, Common Controls v6.
- Everything builds from the command line with one script, `build.ps1` (mirrors `mac-reference/.../build.sh`):
  find VS with `vswhere`, enter the VS dev shell, compile with `cl`/`rc`/`link`, build the `.ico`,
  then compile the installer with `ISCC`. Fail fast on any error.
- Tools: Visual Studio with the C++ workload (installed). Inno Setup: `winget install --id JRSoftware.InnoSetup -e`.

## Mac → Windows mapping
| macOS (main.swift) | Windows |
|---|---|
| `IOServiceAddMatchingNotification` on `IOUSBHostDevice` + VID/PID | `RegisterDeviceNotification` for `GUID_DEVINTERFACE_USB_DEVICE`; on `WM_DEVICECHANGE` `DBT_DEVICEARRIVAL` / `DBT_DEVICEREMOVECOMPLETE`, match `dbcc_name` against `VID_046D&PID_0A89` (case-insensitive). Windows can't filter by VID/PID at registration; string filter is fine. |
| initial `drain` + `headsetArrived` for an already-connected headset | at startup enumerate present interfaces (`CM_Get_Device_Interface_List`) and count matches |
| `connectedCount` | same counter |
| `Headset.worker` serial queue + `Process("headsetcontrol -s 0")` | one worker thread running the native HID send (below); post the result back with `PostMessage(WM_APP+n)`. Coalesce requests while one is running. |
| `NSStatusItem` + `NSMenu` | `Shell_NotifyIconW` (`NOTIFYICON_VERSION_4`) + `TrackPopupMenu` |
| `appearsDisabled` when disconnected | dimmed icon variant (see Icon) |
| `SMAppService.mainApp.register()` | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value, written by the installer |
| Uninstall: confirm → unregister → Trash → `exit(0)` | confirm → launch Inno's `unins000.exe` → exit immediately |
| bundle watcher (dragged to Trash) | not needed: the installer owns removal |
| first-run alert | first-run balloon notification (see First run) |
| SF Symbol `headphones` | Segoe Fluent Icons `U+E7F6` "Headphone" (Win11); fallback Segoe MDL2 Assets `U+E7F6` (Win10) |

## Turning sidetone off (native HID)
Protocol facts come from Sapd/HeadsetControl, which is **GPL-3.0**: write this code from scratch and use
only the protocol bytes; don't copy their source. References: `lib/devices/logitech_g633_g933_935.hpp`
(`setSidetone`) and `lib/devices/protocols/hidpp_protocol.hpp` (`sendHIDPPFeature`).

- **Packet:** HID++ long message, 20 bytes, zero-filled: `11 FF 07 1A 00 00 … 00`
  (`0x11` long report ID, `0xFF` receiver, `0x07 0x1A` sidetone command, `0x00` level). Level range is 0–100; we only send 0.
- **Transport:** a plain output report (`hid_write` in hidapi = `WriteFile`), **not** a feature report.
  No response is read.
- **Target collection:** the G635 exposes several HID collections; use the one with UsagePage `0xFF43`, Usage `0x0202`.

Steps:
1. Get the HID interface class GUID with `HidD_GetHidGuid`; list present interfaces with `CM_Get_Device_Interface_List`.
2. Keep paths containing `vid_046d&pid_0a89` (case-insensitive).
3. For each: `CreateFileW(path, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, …, OPEN_EXISTING)` (access 0 is enough to
   query caps); `HidD_GetPreparsedData` + `HidP_GetCaps`; keep the one with UsagePage `0xFF43` and Usage `0x0202`.
4. Reopen it with `GENERIC_READ|GENERIC_WRITE` and `FILE_FLAG_OVERLAPPED`.
5. Buffer of `caps.OutputReportByteLength` bytes (expect 20; if it's smaller than 5, fail), zero-filled, starting `11 FF 07 1A 00`.
6. `WriteFile`, overlapped, 1 s timeout (`CancelIoEx` on timeout) so a stuck device can't hang the worker.
   Success = all bytes written.
7. Close every handle each time. Don't keep the device open between sends.
8. Retry: up to 5 attempts, 1 s wait before each (Mac behavior). "Collection not found" and write errors both retry.

No admin needed: Windows only locks keyboard/mouse collections for exclusive use, not vendor-defined pages.

## Behavior
### Window
Hidden **top-level** window (never shown), not `HWND_MESSAGE`: message-only windows don't get broadcasts,
and the tray needs the `TaskbarCreated` broadcast to re-add its icon after Explorer restarts.

### Menu — same menu on left and right click
```
<status line>
─────────────
Set sidetone off again
─────────────
Uninstall…
Quit
```
Use `SetForegroundWindow` before `TrackPopupMenu` and post `WM_NULL` after, so the menu closes when clicking elsewhere.

| State | Status line | Status item | "Set sidetone off again" | Tray icon |
|---|---|---|---|---|
| disconnected | `Logitech G635 disconnected` | grayed | grayed | dimmed |
| applying | `Logitech G635 connected — turning off sidetone…` | normal, click does nothing | grayed | normal |
| sidetoneOff | `Logitech G635 connected — sidetone off` | normal, click does nothing | enabled | normal |
| failed | `Logitech G635 connected — couldn't turn off sidetone` | normal, click does nothing | enabled | normal |

Tooltip: `Sidetone Off — <same status>`.

### When to turn sidetone off
- Headset arrival (and at startup if already connected).
- "Set sidetone off again".
- Resume from sleep (`WM_POWERBROADCAST` / `PBT_APMRESUMEAUTOMATIC`) if connected. The Mac version doesn't do this; it's cheap insurance.
- If unplugged while the worker runs, ignore the result (the Mac version does `guard connectedCount > 0`).

### Icon
- Tray: render the headphone glyph at runtime into a 32-bpp premultiplied-alpha `HICON` sized
  `GetSystemMetricsForDpi(SM_CXSMICON, dpi)`. Color follows the taskbar theme
  (`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\SystemUsesLightTheme`: 0 → white, 1 → black).
  Disconnected = same glyph at ~40% opacity. Re-render on `WM_SETTINGCHANGE` ("ImmersiveColorSet") and DPI changes.
- App icon (`.ico` for the exe, installer and Apps list): `make-icon.ps1` (System.Drawing), mirroring
  `make-icon.swift`: rounded square filling 80% of the canvas, corner radius 22.5% of its width,
  vertical gradient `#665CF2` (top) → `#3D2EB2` (bottom), white headphone glyph at ~50% of the tile.
  Sizes 16, 20, 24, 32, 48, 64, 256 (256 PNG-compressed).
- **Do not** reuse the PNGs in `mac-reference/.../build/AppIcon.iconset`: they contain an SF Symbol,
  whose license only covers Apple platforms.

### Uninstall…
Confirmation dialog (`TaskDialogIndirect` or `MessageBoxW`), warning icon. Title "Uninstall Sidetone Off?",
text: "It will be removed from startup and deleted. Your headset's sidetone won't be turned off
automatically anymore." Buttons Uninstall / Cancel, **default = Cancel** (Enter must not confirm by
accident, same as Mac). On confirm: remove the tray icon, close the mutex handle (so Inno's `AppMutex`
check doesn't see the app as running), `ShellExecute` `{app}\unins000.exe /SILENT`, exit.

### Quit
**OPEN — ask me.** The Mac version only quits; it starts again at next login. Windows default unless I say
otherwise: remove the tray icon and exit; the Run entry stays; a Start Menu shortcut reopens it.

### Single instance
Named mutex `Local\SidetoneOff.SingleInstance`; a second launch exits silently. Use the same name as Inno's `AppMutex`.
Exception: `--diag` (below) runs even while the tray instance is running.

### First run
Balloon notification: "Sidetone Off is running — it turns off your G635's sidetone whenever it's connected.
Find the headphones icon in the system tray." Show it once (flag in `HKCU\Software\SidetoneOff`).
Windows 11 puts new tray icons in the overflow; I pin it via Settings › Personalization › Taskbar ›
Other system tray icons. Don't try to force it.

### Logging
`OutputDebugStringW` with the prefix `Sidetone Off:` (same messages as the Mac `NSLog`s, plus HID details:
paths matched, UsagePage/Usage, `OutputReportByteLength`, bytes written, `GetLastError`). No log files.

### Diagnostics (for the first live test)
`SidetoneOff.exe --diag`: no tray, no mutex. Lists every HID collection for VID `046D` / PID `0A89` with path,
UsagePage/Usage and input/output/feature report lengths; marks the one it picked; does one send (no retries)
and reports success or the `GetLastError` code with its `FormatMessageW` text. Shows everything in a
`MessageBoxW` (Ctrl+C copies its text) and exits with code 0 on success, 1 on failure.

## Installer (Inno Setup, `installer/SidetoneOff.iss`)
- Per-user, no UAC: `PrivilegesRequired=lowest`, `DefaultDirName={autopf}\Sidetone Off` (→ `%LOCALAPPDATA%\Programs`).
- Files: `SidetoneOff.exe` only.
- Registry: HKCU Run value `Sidetone Off` = `"{app}\SidetoneOff.exe"`, `uninsdeletevalue`. Delete `HKCU\Software\SidetoneOff` on uninstall.
- Start Menu shortcut. No desktop icon.
- `AppMutex` = the app mutex. In `[UninstallRun]`, kill the app first (`taskkill /IM SidetoneOff.exe /F`, `runhidden`, with a `RunOnceId`).
- `[Run]` start the app after install (`postinstall nowait skipifsilent`).
- Must appear in Settings › Apps › Installed apps, with the app icon.

## Old setup to remove (needs admin, so the per-user installer can't do it)
In an admin PowerShell:
- Scheduled task `G635 Sidetone Off` (runs as SYSTEM): `schtasks /delete /tn "G635 Sidetone Off" /f`
- HeadsetControl, no longer used: `Remove-Item 'C:\Tools\HeadsetControl' -Recurse -Force`
Show these in the README and in the installer's finish page text.

## Test checklist (I run it; the headset is here)
1. `build.ps1` from a clean clone → exe + setup.exe, no warnings at `/W4`.
2. With the G635 plugged in: `SidetoneOff.exe --diag` → picks UsagePage `0xFF43` / Usage `0x0202`, output length 20,
   send succeeds, and I can hear that sidetone is off. If it fails, fix this before anything else.
3. Install → icon appears, first-run balloon shows once.
4. Plug / unplug the G635 → status and icon change; sidetone is off after plugging in.
5. Start Windows with the headset already connected.
6. Sleep → resume with the headset connected.
7. `taskkill /f /im explorer.exe; start explorer` → tray icon comes back.
8. Launch a second copy → nothing happens.
9. Task Manager: memory < 5 MB, CPU 0% idle.
10. Uninstall from the menu → nothing left: `{app}` folder, Run value, `HKCU\Software\SidetoneOff`, Start Menu shortcut, Installed apps entry.
11. Uninstall from Settings › Apps while the app is running → same result.
