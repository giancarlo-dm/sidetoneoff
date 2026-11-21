# Sidetone Off — Windows 0.0 — Plan

Written 2026-11-21, after reading `spec.md` and the Mac source (`mac/`), and after probing the real G635 on this PC.
`spec.md` stays the base. **Where this plan differs, this plan wins**: section 1 has the decisions and section 3 every change from the spec.
The work is split into phases (section 6). Each phase is one commit, reviewed and committed by you, the engineer.

---

## 1. Decisions

All open questions are answered (2026-11-21).

| # | Question | Decision |
|---|---|---|
| B1 | Repository scope | **One repo at the workspace root** (`mac/`, `windows/`, `thoughts/`), branch `main`, local only. `mac/` has no earlier history, so it goes into the first commit like any other file. |
| M1 | Who commits, and when | **You always commit, by hand.** At the end of each phase I hand over the commit title and description. I never stage or commit. |
| M2 | Write succeeds but the read-back times out | **The attempt counts as failed** and is retried. "Sidetone off" appears only when the headset confirms 0. |
| M3 | Exe run from outside the install dir | **Install dialog asking for confirmation**: Install / Cancel. |
| M4 | `--uninstall` from Settings › Apps | **No second dialog.** Windows already asked. |
| M5 | `Publisher` in Installed apps | **"Giancarlo Dalle Mole"** |
| M6 | Old setup (scheduled task, `C:\Tools\HeadsetControl`) | **It never existed** (it was brainstorming). The spec's "Old setup to remove" section is dropped, and the README says nothing about it. |
| M7 | RGB lighting | **Not in this version.** It's for the next one (section 8). |
| M8 | App icon glyph weight | **Thicken the glyph with a light stroke** so it matches the Mac icon's semibold weight. |
| T1 | C++ standard | **`/std:c++23`**: the newest *stable* standard. It is complete in MSVC 14.52 (GA 2026-11-10, LTS). C++26 only exists partially under `/std:c++latest`, which changes with each compiler update. Move to C++26 once MSVC ships it stable. |
| T2 | Toolchain | **VS 2026 Build Tools 18.12 (MSVC 14.52)**, installed 2026-11-21 with `winget install --id Microsoft.VisualStudio.BuildTools` (VCTools workload + recommended, which brings the newest Windows SDK). **VS 2019 Build Tools is removed** once the new toolchain is validated (Phase 0). |

Decisions made earlier in the conversation (installer, detection, read-back, Quit…) are in section 3.

---

## 2. Workflow

**Per phase:**
1. I implement the phase.
2. I build at `/W4 /WX` with zero warnings and run the phase's checks.
3. **I stop and report:** what changed, the checks I ran with their output, and the steps that need your hands.
4. I hand over the **commit title and description**, plus any special staging command (for example `--chmod=+x`).
5. You review and commit.
6. I start the next phase **only after your go**.

**Ground rules:**
- I never stage, commit, amend or push.
- Hardware steps are yours: plugging and unplugging, listening, sleep, taskbar settings. I say exactly what to do and what you should see.
- If a phase finds something that changes the plan, I stop and ask. The plan.md update goes into that phase's commit.

---

## 3. Changes vs spec

| # | Topic | Spec | This plan | Why |
|---|---|---|---|---|
| 1 | Installer | Inno Setup `setup.exe` | **Self-installing exe**, no Inno (5.3) | Your choice: one file, no build-time tool, same model as the Mac app. |
| 2 | When to send | USB arrival → wait 1 s → up to 5 tries | USB arrival **and** HID-collection arrival wake the worker. The 1 s × 5 retries stay as a fallback (5.2). | The HID collection comes up after the USB device, and on a first plug-in (driver install) that can take longer than 5 s. |
| 3 | Success | All 20 bytes written | Bytes written **and** the headset reads back 0. A read-back timeout counts as a failure (M2). | The G635 answers HID++ reads (verified), so "sidetone off" is confirmed by the headset. |
| 4 | Feature index | Hard-coded `0x07` | Asked from the headset on each attempt via `IRoot.getFeature(0x8300)` | Same result on the G635 (7, verified), and the code stays generic for RGB/EQ in the next version. |
| 5 | Quit | OPEN | **Only quit.** Remove the tray icon and exit. The Run value stays, and the Start Menu shortcut reopens the app. | Same as Mac. |
| 6 | Paths | `mac-reference/Sidetone Off/main.swift` | Mac source is in `mac/`. Windows code goes in `windows/`. | Actual layout. |
| 7 | Toolchain | "Visual Studio with C++ workload", C++20 | **VS 2026 Build Tools (MSVC 14.52) + `/std:c++23`** (T1, T2). `build.ps1` fails if MSVC is older than 14.52. | You asked for the newest C++ possible. VS 2019 (MSVC 14.29) stops at C++20. |
| 8 | Old setup | Remove the task and folder (README + finish page) | **Dropped** (M6) | It never existed. |
| 9 | `--diag` | Lists, sends, reports bytes | Also HID++ version, firmware, feature index, **level before and after** | The read-back answers "is it off?" with no need to listen. |
| 10 | Version info | — | `VERSIONINFO` resource with `FileDescription` "Sidetone Off" | The Win11 toast (balloon) and Startup apps take the name from it. |
| 11 | Commits | — | You commit by hand; I provide title and description (M1). | Your rule. |

UI strings follow `spec.md`. They differ from the Mac's on purpose.

---

## 4. Verified on this PC (2026-11-21)

**Toolchain.** Rechecked after installing VS 2026 Build Tools (T2).

| Need | Found |
|---|---|
| Compiler/linker | VS Build Tools 2026 18.12, **MSVC 14.52.37025**: `cl`, `link`, `lib`, `dumpbin` |
| Windows SDK | **10.0.26100.0** with `rc` and `mt`. Every header the plan uses is present (`hidsdi.h`, `hidpi.h`, `cfgmgr32.h`, `dbt.h`, `usbiodef.h`, `shellapi.h`, `shlobj.h`, `shobjidl.h`, `knownfolders.h`, `commctrl.h`, `winresrc.h`), and so is every `.lib`. |
| Real build test | An exe calling every planned API (`HidD_*`, `CM_*`, `RegisterDeviceNotification`, `Shell_NotifyIconW`, `TaskDialogIndirect`, `IShellLinkW`, `SHGetKnownFolderPath`, …), built with `/std:c++23 /W4 /WX /permissive- /MT /O1 /GL` + `/LTCG` + embedded manifest + `VERSIONINFO`. **107 KB**, and it depends only on system DLLs (no VC++ runtime). |
| C++23 | With `/std:c++23 /W4 /WX`, a test using `std::expected` (including `.transform`), deducing `this`, `if consteval`, `std::byteswap`, `std::to_underlying` and `string_view::contains` compiles and runs correctly. `<format>` is avoided, for size. |
| HID at runtime | The HID++ probe rebuilt with MSVC 14.52 reads the G635 (protocol 4.2, level). |
| Icon tooling | `System.Drawing` + Segoe Fluent Icons render the icon in Windows PowerShell 5.1 and in pwsh 7.6. |
| PowerShell | 5.1 and 7.6; execution policy `Unrestricted` (LocalMachine) |
| git | 2.45.2. Identity is set. `core.autocrlf=input` (system). New repos get `core.filemode=false`, hence the `--chmod=+x` in Phase 0. |
| Defender | Real-time protection, behavior monitoring and tamper protection are on. |

**Interfaces** (G635 plugged in, throwaway probes)
```
USB: \\?\USB#VID_046D&PID_0A89#00000000#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
HID: \\?\HID#VID_046D&PID_0A89&MI_03&Col01#…  UsagePage 0x000C Usage 0x0001  in 33 out 37 feat 0
     \\?\HID#VID_046D&PID_0A89&MI_03&Col02#…  UsagePage 0x000B Usage 0x0001  in 2  out 0  feat 0
     \\?\HID#VID_046D&PID_0A89&MI_03&Col03#…  UsagePage 0xFF43 Usage 0x0202  in 20 out 20 feat 0   ← target
```

**HID++ 2.0** (queried with getters only): protocol 4.2, firmware `U1 30.00 build 0007`.

| Index | Feature | Name |
|---|---|---|
| 0 | 0x0000 | Root |
| 1 | 0x0001 | FeatureSet |
| 2 | 0x0003 | DeviceFwVersion |
| 3 | 0x0005 | DeviceName |
| 4 | 0x8070 | ColorLEDEffects |
| 5 | 0x8010 | GKey |
| 6 | 0x8310 | Equalizer |
| 7 | 0x8300 | Sidetone v0: fn0 get level, fn1 set level (0–100) |

**Behavior**
- Writing `11 FF 07 1A 00 …` (20 bytes, output report, `WriteFile`) works without admin rights.
- **No onboard memory.** There is no onboard-profiles feature and no "save" function. After unplug and replug the headset reads **40**, its power-on default. Setting 0 then reads back 0. The app must re-apply on every connection, as the Mac app does.
- **Sidetone is only audible while some app captures the mic.** To *hear* a test, open `mmsys.cpl` › Recording with the boom mic down. The level is stored either way, and the read-back is the real check.
- The protocol matches HeadsetControl: `0x11` / `0xFF` / 20 bytes, sent with `hid_write`. HeadsetControl is GPL-3.0, so we use only these protocol facts, never its code.

---

## 5. Design

### 5.1 HID++ layer
- **`hidpp.h`** is portable (no Windows headers): constants, the 20-byte long-request builder, and parsing of responses and errors. It gets reused for RGB, EQ and Linux.
- **`hid_windows.cpp`** is the Win32 transport.

**Request format.** `[0x11, 0xFF, featureIndex, (function << 4) | 0x0A, params…]`, zero-filled to `OutputReportByteLength`. Software ID `0x0A` makes the set packet byte-identical to the spec's `11 FF 07 1A 00`.

**Responses.**
- A match has byte 2 = feature index and byte 3 = function/software ID.
- A HID++ error looks like `[0x11, 0xFF, 0xFF, featureIndex, fnSw, errorCode]`.
- Unsolicited reports are skipped.

**One transfer.**
1. Overlapped `WriteFile` with a 1 s timeout, and `CancelIoEx` on timeout.
2. Overlapped `ReadFile` loop until the matching response arrives or a 1 s deadline passes.
3. `HidD_FlushQueue` once after opening.

**One attempt.** All handles are closed at the end.
1. `HidD_GetHidGuid` + `CM_Get_Device_Interface_List`, keeping paths that contain `vid_046d&pid_0a89`.
2. Open each with access 0 and call `HidP_GetCaps`. Pick UsagePage `0xFF43` / Usage `0x0202`.
3. Reopen it `GENERIC_READ|GENERIC_WRITE` with `FILE_FLAG_OVERLAPPED`. Fail if the in or out report length is below 5.
4. `IRoot.getFeature(0x8300)` → index. Fail if it returns 0.
5. `Sidetone.setLevel(0)`.
6. `Sidetone.getLevel()`. **The attempt succeeds only if it returns 0.** A timeout or a non-zero value is a failure (M2).

**Result type.** An attempt returns `std::expected<std::uint8_t, Error>` (C++23): either the level read, or an `Error` with the step, the `GetLastError` value and any HID++ error code. `--diag` and the log print the `Error` as is.

**Retries.** Up to 5 attempts, with every failure retried.

**Logging.** `OutputDebugStringW("Sidetone Off: …")` with the paths, caps, feature index, bytes written, level read, `GetLastError` and HID++ error codes.

### 5.2 Detection, worker, state

**Window.** A hidden top-level window with class `SidetoneOff.Window`. Install and uninstall find the running instance by this class.

**Device notifications.** `RegisterDeviceNotification` twice: for `GUID_DEVINTERFACE_USB_DEVICE` and for the HID GUID.

**Triggers.**
- **USB arrival/removal** matching `VID_046D&PID_0A89` (case-insensitive) updates `connectedCount`. This is the only source of the connected/disconnected state.
- **USB arrival:** state `applying`, then `RequestSend()`.
- **HID arrival** matching the VID/PID: `RequestSend()`. All 3 collections fire, and coalescing merges them.
- **Startup:** count the present USB interfaces. If there are any, `applying` and `RequestSend()`.
- **Resume** (`PBT_APMRESUMEAUTOMATIC`) while connected: `applying` and `RequestSend()`.
- **"Set sidetone off again":** `applying` and `RequestSend()`.

**Worker.** One thread and one auto-reset event, `wake`:
```
loop:
  wait(wake, INFINITE)
  for attempt in 1..5:
      wait(wake, 1000)        // the Mac's 1 s wait; a HID arrival (or any new request) cuts it short
      if attempt succeeds: break
  PostMessage(hwnd, WM_APP_RESULT, ok)
```
A request that arrives during an attempt leaves `wake` signaled, so one extra run follows. That is harmless.

**Result.** On `WM_APP_RESULT`: if `connectedCount == 0`, ignore it (Mac guard). Otherwise the state becomes `sidetoneOff` or `failed`.

States, menu, tooltip and icon dimming follow the spec's table.

### 5.3 Self-install (replaces the spec's Installer section)

**Locations.**
- **Install dir:** `%LOCALAPPDATA%\Programs\Sidetone Off\SidetoneOff.exe` (`FOLDERID_UserProgramFiles`).
- **Run value:** `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, value `Sidetone Off` = `"<install dir>\SidetoneOff.exe"`.
- **Start Menu shortcut:** `FOLDERID_Programs\Sidetone Off.lnk`, created with `IShellLinkW` (`ole32`, `uuid`).
- **Uninstall entry:** `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\SidetoneOff`, which makes it show in Settings › Apps › Installed apps. Values:
  - `DisplayName` "Sidetone Off", `DisplayIcon` (`<exe>,0`), `DisplayVersion`, `Publisher` "Giancarlo Dalle Mole" (M5)
  - `InstallLocation`, `InstallDate`, `EstimatedSize`
  - `UninstallString` = `"<exe>" --uninstall`
  - `NoModify=1`, `NoRepair=1`
- **App state:** `HKCU\Software\SidetoneOff` (the first-run flag).

**Entry points.**

| Launch | Behavior |
|---|---|
| No args, exe **outside** the install dir | Confirmation dialog (M3): "Install Sidetone Off?", or "Update Sidetone Off to x.y?" when already installed, with **Install** / Cancel. Install → install steps. Cancel → exit. |
| `--install` | Install steps with no dialog. Used by `build.ps1 -Install`, like `build.sh`. |
| No args, exe **inside** the install dir | Tray app: mutex, tray, first-run balloon once |
| `--uninstall` | From Settings › Apps. **No dialog** (M4). Uninstall steps. |
| Tray › Uninstall… | Spec's confirmation dialog, default Cancel. Then the uninstall steps in-process. |
| `--diag` | Section 5.5 |

**Install steps.**
1. Close the running instance: find its window, `PostMessage(WM_CLOSE)`, wait up to 5 s on its process, and `TerminateProcess` if it is still there.
2. Create the dir and copy the exe. Skip the copy if this exe is already the installed one.
3. Write the Run value, the shortcut and the Uninstall key.
4. Launch the installed exe.
5. Exit.

**Uninstall steps.**
1. Close the running instance, unless we *are* that instance (tray uninstall).
2. Delete the Run value, the Uninstall key, `HKCU\Software\SidetoneOff` and the shortcut.
3. Spawn a hidden `cmd.exe /d /c ping -n 3 127.0.0.1 >nul & del /f /q "<exe>" & rd "<dir>"`, then exit.
   - `rd` without `/s` only removes an **empty** folder.
   - No alternate-data-stream self-delete tricks.

**Risk.** An unsigned exe that copies itself, adds a Run value and spawns `cmd` to delete itself can trip Defender heuristics.
- **Mitigations:** `VERSIONINFO`, the confirmation dialog, and plain APIs only. Defender stays on during every test.
- **SmartScreen:** local builds have no Mark-of-the-Web, so no SmartScreen. A downloaded unsigned exe would get the "Windows protected your PC" warning.

### 5.4 Tray, icon, first run
As in the spec, with these specifics:
- **Tray icon:** `Shell_NotifyIconW` with `NOTIFYICON_VERSION_4`, identified by `uID`, not `NIF_GUID` (which ties the icon to the exe path). Re-added on `TaskbarCreated`.
- **Glyph:** Segoe Fluent Icons `U+E7F6`, with Segoe MDL2 as fallback.
  - Rendered into a 32-bpp premultiplied `HICON` at the taskbar's DPI.
  - Color follows `SystemUsesLightTheme`; disconnected shows it at ~40% opacity.
  - Re-rendered on theme change and on DPI or display change.
- **App icon:** `make-icon.ps1` → `build/app.ico`, embedded in the exe. Spec design (tile, gradient `#665CF2` → `#3D2EB2`, white glyph), with the glyph **thickened by a light stroke** to match the Mac's semibold weight (M8).
- **First-run balloon:** the spec's text, shown once.

### 5.5 `--diag`
Runs without the mutex or tray, so it can run while the tray instance is running. It makes one attempt with no retries. The report lists:
- the USB interface(s);
- every HID collection for 046D/0A89, with path, usage page and usage, and in/out/feature lengths, marking the one picked;
- HID++ version, firmware, and the index of `0x8300`;
- the level read **before**, the write result (bytes, or `GetLastError` + `FormatMessageW`), and the level read **after**;
- a note: "Sidetone is only audible while an app uses the mic (e.g. Sound › Recording)."

**Output.** A `MessageBoxW` by default (Ctrl+C copies it). `--diag --console` writes to stdout with no dialog, so scripts and agents can use it.

**Exit code.** 0 when the after-level reads 0, otherwise 1.

### 5.6 Layout and build
```
.gitignore  .gitattributes
mac/                   Mac app (unchanged)
thoughts/              spec + plan
windows/
  build.ps1            build (+ -Install); fail fast
  make-icon.ps1        build/app.ico via System.Drawing
  README.md
  src/
    main.cpp           wWinMain, arg dispatch, mutex, window proc, state
    hidpp.h            portable HID++ 2.0 helpers (no Windows headers)
    hid_windows.cpp/.h find collection, transfer, attempt
    worker.cpp/.h      worker thread, coalescing, retries
    devices.cpp/.h     notifications, enumeration, counting
    tray.cpp/.h        icon rendering, tray, menu, balloon
    install.cpp/.h     install / update / uninstall
    diag.cpp/.h        --diag
    strings.h          every UI string in one place (backport / Linux)
    version.h          the single version number (code + rc)
  res/
    app.rc  app.manifest  resource.h
```

**`build.ps1`.**
1. Find VS with `vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`. Build Tools are only found with `-products *`.
2. Prepend the VS Installer dir to PATH. Without it, VsDevCmd prints "'vswhere.exe' is not recognized".
3. `Enter-VsDevShell -arch=x64`.
4. Run `make-icon.ps1`, then `rc`.
5. Fail fast if `$env:VCToolsVersion` is below **14.52**, the first MSVC with stable `/std:c++23`.
6. Compile: `cl /std:c++23 /W4 /WX /permissive- /MT /O1 /GL /EHsc /DUNICODE /D_UNICODE`.
7. Link: `link /LTCG /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:res\app.manifest` with `hid cfgmgr32 user32 gdi32 shell32 comctl32 advapi32 ole32 uuid`.
8. Check `$LASTEXITCODE` after every tool.
9. Print the exe size, and fail if it is over 300 KB.
10. With `-Install`, run `build\SidetoneOff.exe --install`.

Output goes to `windows/build/`, which git ignores.

---

## 6. Phases

Each phase ends with a clean `/W4 /WX` build, my checks, your review, and **one commit made by you** with the title and description I hand over.

### Phase 0: Repository
**Me**
- `git init -b main` at the workspace root (`init.defaultBranch` isn't set; your identity is).
- `.gitignore` (currently empty): `windows/build/`, `mac/build/`, `.DS_Store`, `.vs/`.
- `.gitattributes`: `* text=auto`; `*.sh` and `*.swift` as `eol=lf`; `*.png` and `*.ico` as `binary`.
- This plan, reconciled with the decisions.

**Toolchain (T1, T2).** Not committed, but done in this phase:
1. Install VS 2026 Build Tools.
2. Rerun the toolchain check (section 4) with MSVC 14.52 and `/std:c++23`.
3. Once it passes, uninstall VS 2019 Build Tools (`winget uninstall --id Microsoft.VisualStudio.2019.BuildTools`; UAC prompt).
4. Recheck that `vswhere` finds only VS 2026.

**My checks.** `git status` lists exactly: `.gitattributes`, `.gitignore`, `mac/` (4 files), `thoughts/` (spec + plan).

**You**
- `git add -A`
- `git add --chmod=+x mac/build.sh` (Windows doesn't record the exec bit)
- Commit.

**Commit title.** `Phase 0: Initialize repository with the Mac app, Windows spec and plan`

### Phase 1: Build skeleton
**Changes**
- `build.ps1`.
- `version.h` and `strings.h`.
- `res/app.rc` with `VERSIONINFO`.
- `res/app.manifest`: asInvoker, PerMonitorV2, Common Controls v6, supportedOS Win10/11.
- `main.cpp`: mutex `Local\SidetoneOff.SingleInstance`, the hidden window, the message loop, `WM_CLOSE` → exit.
- No tray and no icon yet.

**My checks**
- A build from a clean `build/` has 0 warnings, and the size is printed.
- `dumpbin /dependents` shows no VC++ runtime.
- The manifest is embedded.
- When run, the process stays alive with no window.
- A second launch exits at once.
- `taskkill` (no `/f`) closes it.

**You.** Review the code, and run `build.ps1` once yourself.

**Commit title.** `Phase 1: Add Windows build script and app skeleton`

### Phase 2: HID++ and `--diag`
**Changes.** `hidpp.h`, `hid_windows.*`, `diag.*`, and the `--diag` dispatch in `main.cpp`.

**My checks.** `--diag --console` with the G635 plugged in should show Col03 picked, out 20, index 7, after-level 0, and exit code 0.

**You**
1. Unplug → I run `--diag` → it exits 1 with "G635 not found".
2. Replug → I run it → before **40**, after **0**.
3. Run `SidetoneOff.exe --diag` by double-click → MessageBox, and Ctrl+C copies the text.

If anything fails here, it gets fixed before Phase 3 (spec).

**Commit title.** `Phase 2: Talk HID++ to the G635 and add --diag`

### Phase 3: Detection, worker, tray menu
**Changes**
- `devices.*`, `worker.*`, and `tray.*` with a stock placeholder icon.
- The state machine in `main.cpp`.
- Resume handling and `TaskbarCreated`.
- Menu: status line / Set sidetone off again / Quit. Uninstall… comes in Phase 5.

**My checks**
- Capture the app's `OutputDebugString` lines with a scratch listener.
- With the headset connected at startup, the log shows the send and the read-back of 0, and `--diag --console` confirms 0.

**You**
1. Unplug and replug, with Sound › Recording open. The status should go disconnected → applying → sidetone off, and the sidetone should be gone within about 2 s.
2. Use "Set sidetone off again".
3. Use Quit.
4. Run `taskkill /f /im explorer.exe; start explorer` → the icon comes back.
5. Sleep and resume → sidetone off.

**Commit title.** `Phase 3: Detect the headset and turn sidetone off from the tray`

### Phase 4: Icons
**Changes**
- Tray glyph rendering: theme, DPI, dimming, re-render.
- `make-icon.ps1` → `app.ico` with sizes 16/20/24/32/48/64/256, glyph thickened (M8), embedded via `rc`.

**My checks**
- `app.ico` has every size.
- I render preview PNGs of the tray icon (light/dark, connected/dimmed, 100–200%) and of the app icon, and look at them.

**You**
1. Switch the taskbar between light and dark → the icon color follows.
2. Set display scale to 150% → the icon stays sharp.
3. Unplug → the icon dims.
4. The exe's icon in Explorer.

**Commit title.** `Phase 4: Draw the tray and app icons`

### Phase 5: Self-install, update, uninstall, first run
**Changes**
- `install.*`.
- The `--install` / `--uninstall` dispatch, and the confirmation dialog for an exe run from outside the install dir (M3).
- Tray › Uninstall… with its dialog.
- The first-run balloon.
- `build.ps1 -Install`.

**My checks**
- `--install` → the folder, Run value, shortcut and Uninstall key (Publisher M5) exist, checked with `reg query` and a dir listing.
- `--uninstall` → all of them are gone, with no dialog (M4).
- Reinstall, then an update while the app is running → the old PID exits and a new PID runs.

**You**
1. Double-click install → confirmation dialog → the balloon shows once.
2. Installed apps shows the entry with the icon and "Giancarlo Dalle Mole", and Startup apps lists it.
3. Tray › Uninstall… → Enter cancels, the button confirms.
4. Settings › Apps › Uninstall while the app is running.
5. Defender › Protection history stays clean.

**Commit title.** `Phase 5: Install, update and uninstall without an installer`

### Phase 6: README and release check
**Changes**
- `windows/README.md`: build, install, uninstall, `--diag`, the mic-in-use note, troubleshooting.
- Update the plan's status.

**My checks**
- Section 7 end to end, starting from a clean state.
- Working set under 5 MB (`Get-Process`).
- CPU time doesn't grow over 60 s idle.

**You.** The hardware items of section 7.

**Commit title.** `Phase 6: Document the Windows app and record the release check`

---

## 7. Release checklist (run in Phase 6; the phase where each item first appears is in brackets)

1. `build.ps1` from a clean clone → `build\SidetoneOff.exe`, no warnings, < 300 KB. [P1]
2. `--diag` with the G635 → Col03 / `0xFF43`/`0x0202`, out 20, index 7, after-level 0, exit 0. [P2]
3. Double-click the exe → confirmation dialog → tray icon. The first-run balloon shows once. The app shows in Installed apps (icon, Publisher) and in Startup apps. [P5]
4. Plug / unplug → the status and icon change. After plugging in, `--diag` reads 0, and with Recording open there's no sidetone. [P3]
5. Restart Windows with the headset connected → sidetone off. [P5]
6. Sleep → resume while connected → sidetone off. [P3]
7. Restart Explorer → the tray icon comes back. [P3]
8. A second launch from the Start Menu → nothing happens. [P1/P5]
9. Run a newer build while the app is running → update dialog, the old instance closes, the new one runs. [P5]
10. Task Manager: < 5 MB, 0% CPU idle. [P6]
11. Tray › Uninstall… (Enter = Cancel) → confirm → nothing left: folder, Run value, Uninstall key, `HKCU\Software\SidetoneOff`, shortcut. [P5]
12. Settings › Apps › Uninstall while running → same result, no extra dialog. [P5]
13. Defender on throughout → nothing in Protection history. [P5/P6]

---

## 8. Future (next versions)

- **RGB** via `0x8070` ColorLEDEffects. Not in this version (M7); it's the next one. The G635 lists the feature, and HeadsetControl already drives its two zones, logo and side strips. It is probably volatile too, so the same re-apply-on-connect path would cover it. The UI would be a submenu of presets plus `ChooseColorW`.
- **Equalizer** via `0x8310`.
- **Other headsets in the family:** G633 `0A5C`, G933 `0A5B`, G935 `0A87`, G733 `0AB5`/`0AFE`/`0B1F`. HeadsetControl uses the same protocol for all of them; it's a PID table, but untested here.
- **Backport to Mac:**
  - native HID (IOKit `IOHIDDevice`) instead of `headsetcontrol`;
  - read-back;
  - resume trigger;
  - HID-arrival trigger.
- **Linux:** reuse `hidpp.h`, with `hidraw` + udev monitor for the transport, StatusNotifierItem for the tray, and its own glyph (the Segoe glyph is Windows-only).
