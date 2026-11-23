# SidetoneOff

SidetoneOff turns off the **sidetone** of a **Logitech G635** headset (hearing your own voice in the headphones) every time the headset is connected, so you don't need Logitech G HUB for it.

The G635 doesn't remember its sidetone level: it goes back to its default whenever it loses power. SidetoneOff runs quietly in the menu bar or system tray and turns sidetone off again each time the headset is plugged in. The Windows app also does it when the computer wakes from sleep.

| Platform | Version | How it talks to the headset |
|---|---|---|
| **Windows 10/11** | 1.0.0 | Directly, over Logitech's HID++ 2.0 protocol, and it confirms the change with the headset. One self-installing exe, no dependencies. |
| **macOS 13+** | 1.0 | Through [HeadsetControl](https://github.com/Sapd/HeadsetControl). |
| **Linux** | planned | See [Future](#future). |

## Install

### Windows

1. Download **[SidetoneOff-windows-x64.exe](https://github.com/giancarlo-dm/sidetoneoff/releases/latest/download/SidetoneOff-windows-x64.exe)** from the [latest release](https://github.com/giancarlo-dm/sidetoneoff/releases/latest), run it, and choose **Install**. The exe isn't code-signed, so Windows may say "Windows protected your PC": choose **More info › Run anyway**. It installs for your account only (no administrator rights), in `%LOCALAPPDATA%\Programs\SidetoneOff`, adds itself to startup, the Start menu and Installed apps, and starts.
2. Look for the headphones icon in the system tray. Windows 11 puts new icons in the **^** overflow; to keep it visible, use Settings › Personalization › Taskbar › Other system tray icons.

The tray menu shows the headset's status and has **Set sidetone off again**, **Show log**, **Uninstall…** and **Quit**. Hovering over the icon shows the status too.

**Update** by running a newer `SidetoneOff.exe` and choosing **Update**. **Uninstall** with Tray › Uninstall…, or Settings › Apps › Installed apps › SidetoneOff › Uninstall.

More, including troubleshooting and diagnostics: [`windows/README.md`](windows/README.md).

### macOS

There's no download for macOS yet: build it from this repository.

1. Install HeadsetControl with [Homebrew](https://brew.sh): `brew install headsetcontrol`. The app expects it in `/opt/homebrew/bin`, Homebrew's location on Apple Silicon Macs.
2. Build and install the app (see [Develop › macOS](#macos-1)). It goes into `/Applications`, adds itself to your login items, and opens.

The menu bar icon's menu shows the headset's status and has **Turn Off Sidetone Again**, **Uninstall SidetoneOff…** and **Quit SidetoneOff**. **Uninstall** from that menu, or by dragging the app to the Trash.

**Good to know:** sidetone is only audible while some app is using the microphone. To test, open your sound settings' input page with the mic boom down.

## Develop

```
mac/       the macOS app (Swift)
windows/   the Windows app (C++23, Win32)
claude/    specs, plans and the decisions behind each release
```

### Windows

You need the Visual Studio 2026 Build Tools with the C++ workload (MSVC 14.52 or newer) and PowerShell 5.1 or 7:

```
winget install --id Microsoft.VisualStudio.BuildTools -e --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --includeRecommended"
```

Then, from the repository root:

```
.\windows\build.ps1            # builds windows\build\SidetoneOff.exe
.\windows\build.ps1 -Install   # also installs it (or updates the installed one) and starts it
```

Every build starts clean, treats all warnings as errors, and runs the unit tests: a failing test stops the build.

- **Code:** in `windows/src`, the HID++ protocol (`hidpp.*`), what gets applied to the headset (`profile.*`) and the log history (`log_history.*`) are portable C++. The rest is the Windows side: the HID transport, device detection, the tray, the installer and the log window.
- **Tests:** `windows/tests/tests.cpp` runs them against a fake headset, so they need no hardware.
- **Diagnostics:** `.\windows\build\SidetoneOff.exe --diag --console | Out-Host` reports every step of one attempt, including the raw HID++ traffic. Tray › Show log shows what the running app did.
- **Design:** every decision is recorded in [`claude/2026-11-13_1-windows-1.0.0/plan.md`](claude/2026-11-13_1-windows-1.0.0/plan.md).

### macOS

You need the Xcode Command Line Tools (`xcode-select --install`) and HeadsetControl (`brew install headsetcontrol`). Then:

```
./mac/build.sh
```

It compiles `main.swift`, draws the app icon (`make-icon.swift`), assembles and signs `SidetoneOff.app` (ad hoc), replaces the copy in `/Applications`, and opens it. The app logs to Console.app; filter for `SidetoneOff`.

## Future

- **The Windows 1.0 design, on the Mac.** Talk HID++ to the headset directly (IOKit) instead of through HeadsetControl, so there's nothing else to install. Confirm the change by reading the level back. Re-apply after waking from sleep and as soon as the headset's HID interface appears. Use the drawn headphones app icon, since Apple's SF Symbols aren't allowed in app icons.
- **RGB lighting.** Set the colors of the G635's logo and side light strips (HID++ feature `0x8070`) from the tray menu, with a few presets and a color picker. Like sidetone, they'll be re-applied every time the headset connects.
- **Linux, for Bazzite and Kubuntu/Ubuntu.** C++23, sharing the Windows app's HID++ core, with a tray icon that works on KDE Plasma and on Ubuntu's GNOME. The plan is to install it by downloading and double-clicking: a Flatpak on Bazzite and other KDE systems, a `.deb` on Ubuntu and Kubuntu. Uninstalling would be from the app store or the tray menu. Installing would ask for your password once, to let your user account talk to the headset.

The plans also mention an equalizer and other Logitech headsets of the same family.

## Credits

The HID++ details of the G635 come from [HeadsetControl](https://github.com/Sapd/HeadsetControl), which the macOS app uses today. The Windows app contains none of HeadsetControl's code; it implements the protocol from scratch.
