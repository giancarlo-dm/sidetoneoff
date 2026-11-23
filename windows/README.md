# SidetoneOff for Windows

SidetoneOff turns off the sidetone of a Logitech G635 headset (your own voice in the headphones) every time the headset is connected. It lives in the system tray, starts when you sign in, and does this one job of Logitech G HUB without it. It talks to the headset directly: there is nothing else to install.

**Requires:** Windows 10 or 11 (x64), and a Logitech G635 connected by USB. No administrator rights.

## Install

1. Run `SidetoneOff.exe` and choose **Install**.
2. It installs for your account only, in `%LOCALAPPDATA%\Programs\SidetoneOff`, adds itself to startup and to the Start menu, and starts. A notice says "SidetoneOff is running".
3. Windows 11 puts new tray icons in the **^** overflow. To keep the headphones icon on the taskbar: Settings › Personalization › Taskbar › Other system tray icons › SidetoneOff.

The exe isn't signed, so a copy downloaded from the internet may show "Windows protected your PC". Choose More info › Run anyway.

## Use

The tray icon is a pair of headphones, dimmed while the headset is disconnected. Hover over it to see the status. Click it, left or right, for the menu:

| Menu item | What it does |
|---|---|
| *Status line* | Shows the headset's state (below); not clickable |
| Set sidetone off again | Turns sidetone off now |
| Show log | Shows what the app did since it started |
| Uninstall… | Removes the app (asks first) |
| Quit | Stops the app until you sign in again, or start it from the Start menu |

| Status | Meaning |
|---|---|
| Logitech G635 disconnected | No headset |
| Logitech G635 connected — turning off sidetone… | Working on it |
| Logitech G635 connected | Sidetone is off: the headset confirmed it |
| Logitech G635 connected — couldn't turn off sidetone | Five attempts failed; see Troubleshooting |

The G635 doesn't remember its sidetone level: it goes back to its default whenever it loses power. So SidetoneOff turns it off each time the headset is connected, when the app starts, and when the PC wakes from sleep.

## Update

Run the newer `SidetoneOff.exe` and choose **Update**. The running copy closes and the new one starts.

## Uninstall

Use Tray › Uninstall…, or Settings › Apps › Installed apps › SidetoneOff › Uninstall. Either removes everything the app added (see [What it changes on your PC](#what-it-changes-on-your-pc)).

## Troubleshooting

- **I still hear myself.** Sidetone is only audible while some app uses the microphone, with the mic boom down. To test, open Sound settings with `mmsys.cpl`, go to the Recording tab, and speak. If you hear yourself, choose **Set sidetone off again**, then check **Show log**.
- **"couldn't turn off sidetone".** Unplug and replug the headset, or choose **Set sidetone off again**. **Show log** says which step failed, and `--diag` (below) shows the full exchange with the headset.
- **I can't find the icon.** Look in the **^** overflow on the taskbar. After Explorer restarts, the icon comes back there too.
- **Logitech G HUB.** G HUB may apply its own sidetone level when the headset connects. If it's installed, quit or uninstall it.
- **Other headsets.** Only the G635 (USB ID `046D:0A89`) is supported.

## Diagnostics

**Show log** in the tray menu lists everything the running app did since it started: connections, attempts, results and errors, each with its time. Ctrl+A and Ctrl+C copy it.

**`--diag`** makes one attempt and reports every step. It works while the tray app is running. Open PowerShell in the folder with the exe (the installed one is in `%LOCALAPPDATA%\Programs\SidetoneOff`):

```
.\SidetoneOff.exe --diag                      # the report in a dialog; Ctrl+C copies it
.\SidetoneOff.exe --diag --console | Out-Host # the report as text; the exit code is 0 when sidetone is off
```

A healthy report reads like this (shortened):

```
HID collections:
  \\?\HID#VID_046D&PID_0A89&MI_03&Col03#…
    usage page FF43, usage 0202, input 20, output 20, feature 0   <- picked
HID++ protocol 4.2
Firmware U1 30.00 build 0007
Sidetone feature 0x8300 at index 7
Sidetone level before: 40
Set level 0: 20 bytes written, acknowledged by the headset
Sidetone level after: 0
Result: sidetone is off.
```

The level before is 40, the G635's default, right after it is plugged in, or 0 if the app had turned sidetone off already.

The app also writes `SidetoneOff: …` lines to the Windows debug output, which tools such as DebugView show.

## Build from source

You need the Visual Studio 2026 Build Tools with the C++ workload (MSVC 14.52 or newer) and PowerShell 5.1 or 7:

```
winget install --id Microsoft.VisualStudio.BuildTools -e --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --includeRecommended"
```

Then, from the repository root:

```
.\windows\build.ps1            # builds windows\build\SidetoneOff.exe
.\windows\build.ps1 -Install   # also installs it (or updates the installed one) and starts it
```

Each build starts from an empty `windows\build`, draws the icons, compiles with all warnings as errors, and runs the unit tests: a failing test stops the build. The build also fails if the exe grows past 300 KB. If PowerShell refuses to run the script, use `powershell -ExecutionPolicy Bypass -File .\windows\build.ps1`.

Running `windows\build\SidetoneOff.exe` with no arguments offers to install it, as any copy outside the install folder does. These are all its arguments:

| Arguments | What they do |
|---|---|
| *(none)* | Runs the app from the install folder; from anywhere else, offers to install or update |
| `--install` | Installs or updates with no dialog |
| `--uninstall` | Uninstalls with no dialog (Settings › Apps uses this) |
| `--autostart` | Runs the app without the "running" notice (the start at sign-in uses this) |
| `--diag` / `--diag --console` | The report described under Diagnostics |

## How it works

- The G635 speaks Logitech's HID++ 2.0 protocol on one of its USB HID interfaces (usage page `FF43`, usage `0202`).
- SidetoneOff asks the headset where its Sidetone feature (`0x8300`) is, sets the level to 0 (the 20-byte report `11 FF 07 1A 00 …`), and reads the level back. An attempt counts only when the headset confirms 0. It tries up to 5 times, 1 s apart.
- It reacts to Windows' device notifications (the headset's USB device and HID interfaces arriving) and to waking from sleep. It never polls.
- The protocol facts come from [HeadsetControl](https://github.com/Sapd/HeadsetControl) (GPL-3.0); none of its code is used.

The design and every decision behind it are in [`thoughts/2026-11-13_1-windows-0.0/plan.md`](../thoughts/2026-11-13_1-windows-0.0/plan.md).

## What it changes on your PC

Everything is per user, and uninstalling removes all of it.

| What | Where |
|---|---|
| The app | `%LOCALAPPDATA%\Programs\SidetoneOff\SidetoneOff.exe` |
| Start at sign-in | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, value `SidetoneOff`; plus Windows' on/off switch for it under `…\Explorer\StartupApproved\Run`, if you used it |
| Start menu shortcut | `SidetoneOff.lnk` in your Start menu's Programs folder |
| Installed apps entry | `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\SidetoneOff` |
| Settings | `HKCU\Software\SidetoneOff` (empty in this version) |

No log files, no services, no drivers.
