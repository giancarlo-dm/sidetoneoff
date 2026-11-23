# SidetoneOff — Windows 0.0 — Plan

Written 2026-11-21, after reading `spec.md` and the Mac source (`mac/`), and after probing the real G635 on this PC.
`spec.md` stays the base. **Where this plan differs, this plan wins**: section 1 has the decisions, and section 3 every change from the spec.
The work is split into phases (section 6). Each phase is one commit, reviewed and committed by you, the engineer.

**Status (2026-11-23):** Phases 0–6 are committed; Phase 7 records the release check of **1.0.0** (section 7). No open questions remain.

---

## 1. Decisions

Answered 2026-11-21 unless noted.

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
| M8 | App icon glyph weight | **About the Mac icon's semibold weight.** The Segoe glyph got a light outline for it (Phase 4). The drawn glyph (M12) has that weight built in: a 2-unit stroke on its 24-unit grid. |
| M9 | Release version | **1.0.0** (answered 2026-11-23, at the start of Phase 7), matching the Mac app's 1.0. Development ran as 0.1.0; the release checklist (section 7) runs on the 1.0.0 build. |
| T1 | C++ standard | **`/std:c++23`**: the newest *stable* standard. It is complete in MSVC 14.52 (GA 2026-11-10, LTS). C++26 only exists partially under `/std:c++latest`, which changes with each compiler update. Move to C++26 once MSVC ships it stable. |
| T2 | Toolchain | **VS 2026 Build Tools 18.12 (MSVC 14.52)**, installed 2026-11-21 with `winget install --id Microsoft.VisualStudio.BuildTools` (VCTools workload + recommended, which brings the newest Windows SDK). **VS 2019 Build Tools is removed** once the new toolchain is validated (Phase 0). |
| A1 | Code structure, with RGB and other features in mind | **Hybrid** (decided after Phase 1). Classes for what owns a resource or keeps state, free functions for operations. One interface, `Transport`, because there really are several implementations (Windows now; Linux, Mac and tests later). No inheritance hierarchies. Errors are `std::expected` values, not exceptions. See 5.0. |
| L1 | Conversation language | **English** from 2026-11-21, at your request. This overrides the spec's pt-BR. Windows UI paths stay in their English names. |
| M10 | Unit tests with a fake `Transport` (5.0) | **Yes** (answered at the start of Phase 2). `windows/tests/tests.cpp` builds into one small test exe, with plain checks and no framework. `build.ps1` builds and runs it on every build, before the app is linked, and a failing test fails the build. The tests need no headset. They cover the request bytes (`11 FF 07 1A 00 …`), response matching, skipping unsolicited reports, HID++ errors, a missing feature, timeouts, write errors and the error text. A read-back that isn't 0 is covered in Phase 3, with `Apply`. |
| M11 | App icon at small sizes (found in Phase 4: with the spec's proportions, the glyph is a blur at 16 and 20 px) | **The Mac's layout at every size**, as in the spec. Phase 4 shipped a small-size layout for 16–24 px (a full tile with a pixel-hinted glyph). It was reverted in the Phase 5 review (2026-11-23): you found it ugly, and want the icon as on the Mac. |
| M12 | Tray and app icon glyph (Phase 5 review: the Segoe glyph looks square next to the Mac's rounded one) | **An original drawing**: a rounded headband with filled, rounded ear cups (variant B of the drawn prototypes), drawn by `make-icon.ps1` for both icons. The Mac's SF Symbols "headphones" can't ship on Windows: Apple licenses SF Symbols for Apple platforms only, and never in app icons. Windows' icon font has no rounder headphone. |
| M13 | Tooltip and status wording (Phase 5 review) | **Tooltip on two lines**: "SidetoneOff", then the status. When sidetone is off, the status is just "Logitech G635 connected", in the tooltip and in the menu. |
| M14 | When "SidetoneOff is running" shows (Phase 5 review: it didn't after an update, or after Quit and reopening) | **On every start you make** (install, update, Start menu, double-click), **not at sign-in**: the Run value passes `--autostart`, which skips it. A start while the app is already running still does nothing (spec). |
| M15 | Log window: where the history lives (asked 2026-11-23, after Phase 5) | **In memory**: the last 500 lines since the app started, lost on Quit or restart. No log files, as in the spec. You asked to add the phase and continue, so this and M16–M17 are my recommendations. |
| M16 | Log window: which lines | **The same lines as the debug log**, each after its date and time. One source of messages; some lines are technical (e.g. the HID device path). |
| M17 | Log window: dark mode | **No.** Standard Windows controls, which are light-themed; dark mode needs undocumented APIs. |
| M18 | The app's name (Phase 7: you noted "the app name is SidetoneOff"; the spec and the Mac app display "Sidetone Off") | **"SidetoneOff", one word, everywhere on Windows** (answered 2026-11-23): dialogs, tooltip, notice, log window title, Installed apps, Start menu shortcut, install folder, Run value, version info, debug-log prefix and README. Names that already were one word stay (exe, window classes, mutex, `Uninstall\SidetoneOff`, `Software\SidetoneOff`). The repo stays `sidetone-off`. The Mac app still displays "Sidetone Off" until it's renamed too (section 8). |

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

**Code for the reviewer.** You know Java, TypeScript and some C, not C++. So:
- The code stays plain modern C++: no template metaprogramming, no operator overloading of our own, no clever tricks.
- A C++ idiom gets a one-line comment where it first appears.
- Each phase report has a short **C++ notes** part that maps the idioms new in that phase to Java/TS. The glossary in 5.0 covers the common ones.

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
| 10 | Version info | — | `VERSIONINFO` resource with `FileDescription` "SidetoneOff" | The Win11 toast (balloon) and Startup apps take the name from it. |
| 11 | Commits | — | You commit by hand; I provide title and description (M1). | Your rule. |
| 12 | Conversation language | pt-BR | **English** (L1) | Your request. |
| 13 | Connected state | `connectedCount` counter, as on the Mac | Re-read from the system on each G635 removal (5.2) | A counter can drift if a notification is missed. |
| 14 | Open menu | — | **Updates live**: it reopens in place when the state changes (5.4) | Found in the Phase 3 review: an unplug while the menu was open left it stale. The Mac menu updates live. |
| 15 | Glyph | Segoe Fluent Icons `U+E7F6`, rendered at runtime | **An original drawn headphones glyph** (M12), drawn at build time into the app icon and into `tray.ico`, which the app recolors | The Segoe glyph looks square; the Mac's rounded SF Symbol can't ship on Windows. |
| 16 | Tray icon alpha | "32-bpp premultiplied-alpha `HICON`" | **Straight (not premultiplied) alpha** | Measured in Phase 4: `DrawIconEx` blends icon pixels as straight alpha, so premultiplied pixels would draw darker edges. |
| 17 | PNG images in `app.ico` | 256 px only | **48, 64 and 256 px** | Windows reads PNG icons at any size, and this saves about 20 KB of the 300 KB budget. 16–32 px stay plain bitmaps. |
| 18 | First-run notice | One sentence, shown once | The same words as a title, "SidetoneOff is running", plus the rest as text, with the app icon (5.4). Shown **on every start you make, not at sign-in** (M14). | Windows 11 shows the notice as a toast with a bold title. You want it after an update and after reopening too. |
| 19 | Uninstall cleanup | Run value, settings key, shortcut, files | Also the StartupApproved value, Windows' record of the switch in Settings › Apps › Startup | "Nothing left" (section 7, #11), including what Windows wrote for the app. |
| 20 | Tooltip and status | Tooltip `Sidetone Off — <status>`; "… connected — sidetone off" | Tooltip on two lines; "Logitech G635 connected" when sidetone is off (M13) | "Sidetone off" twice in one line read oddly. |
| 21 | Run value | `"<exe>"` | `"<exe>" --autostart` | Tells the start at sign-in apart, so it shows no notice (M14). |
| 22 | Menu status line | Grayed while disconnected, otherwise normal with a click that does nothing | **Always disabled** (grayed, not clickable) | It's a status, not a command (your Phase 5 review). The Mac's status line is always disabled too. |
| 23 | Logging | `OutputDebugStringW` only, no log files | Also a **log window** (Tray › Show log) with the history since the app started. Still no files (M15). | Without a debug listener such as DebugView, there was no way to see what the app did. |
| 24 | App name | "Sidetone Off" (tooltip, dialogs, balloon, install folder, Run value) | **"SidetoneOff"** everywhere (M18) | Your call: the app's name is one word. |

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

### 5.0 Code structure (A1)

**Rule.**
- Make it a **class** when it owns a resource (a handle, a thread, a registration) or keeps state that has rules.
- Make it a **free function** when it is an operation.
- Use **`virtual`** only where several implementations really exist. Today that is only `Transport`.
- **No inheritance hierarchies.**
  - Headset models (G635, G935, G733…) are a PID table, not classes. HID++ 2.0 reports its features at runtime, so no model needs code of its own.
  - Headset features (sidetone, RGB, EQ) are functions, not an `IFeature` hierarchy.

**Where change is expected, and what absorbs it**

| Changes | Example | Absorbed by |
|---|---|---|
| Headset feature | sidetone → RGB → EQ | A new free function on `Device&`, plus a field in `Profile` |
| Platform | Windows → Linux (hidraw) → Mac (IOKit) | A new class implementing `Transport` |
| Headset model | G635 → G935, G733… | A row in the PID table |
| Win32 resources | `HANDLE`, `HKEY`, `HICON`, `HDEVNOTIFY` | RAII wrappers (`win32.h`) |
| App state | window, tray, connection state | The `App` class |

**Main types**

| Type | Kind | Owns / does | File | Phase |
|---|---|---|---|---|
| `hidpp::Transport` | interface | Writes one report, and reads one report with a deadline. Implemented by `WinHidTransport`, by the tests' `FakeTransport`, and by `--diag`'s `RecordingTransport`, which wraps the real one to log the traffic. | `hidpp.h` | 2 |
| `hidpp::Device` | class | Builds requests, matches responses, skips unsolicited reports, caches feature indexes | `hidpp.*` | 2 |
| `hidpp::GetSidetone`, `SetSidetone` (later `SetLeds`, EQ) | free functions on `Device&` | One HID++ call each | `hidpp.*` | 2 |
| `WinHidTransport` | class implementing `Transport` | Finds and opens the G635's HID++ collection and does the overlapped I/O. Closes its handle when destroyed. | `hid_windows.*` | 2 |
| `UniqueHandle`, `UniqueHkey`, … | RAII wrappers | Close Win32 handles automatically | `win32.h` | 2 |
| `Profile` + `Apply()` | struct + free function | What to apply on each connection. In this version that is `sidetone = 0`. | `profile.*` | 3 |
| `Worker` | class | The thread, the `wake` and `stop` events, and the retries | `worker.*` | 3 |
| `DeviceWatcher` | class | The two device-notification registrations and the connected count | `devices.*` | 3 |
| `Tray` | class | The tray icon, menu and balloon. Keeps the normal and dimmed icons, and redraws them when the taskbar changes. | `tray.*` | 3, 4 |
| `CurrentTaskbarStyle`, `RenderTrayIcon` | free functions | Read the taskbar's icon size and theme; make the glyph's `HICON` in the taskbar's color | `tray_icon.*` | 4 |
| `App` | class | The hidden window, its window proc and the state machine. Owns `Worker`, `DeviceWatcher` and `Tray`. | `app.*` | 3 |
| `RunDiag`, install / uninstall | free functions | Nothing long-lived to own | `diag.*`, `install.*` | 2, 5 |
| `LogHistory` | class, portable | The last 500 log lines, and which ones a reader has seen. Thread-safe. | `log_history.*` | 6 |
| `LogWindow` | class | The log window, its text box and font. Owned by `App`. | `log_window.*` | 6 |

**Errors.**
- Device and Windows failures are values, not exceptions: anything that can fail returns `std::expected<T, Error>`.
- A constructor can't return a value, so a type that can fail to open uses a static factory instead: `static std::expected<WinHidTransport, Error> Open()`.

**C++ for a Java / TypeScript reader**

| C++ | Closest Java / TypeScript |
|---|---|
| A class with only pure `virtual` functions (`Transport`) | `interface` |
| `std::expected<T, Error>` | A `Result<T, E>`, or in TS `{ ok: true, value } \| { ok: false, error }`. Test it with `if (!result)`, then read `*result` or `result.error()`. |
| `std::optional<T>` | `Optional<T>` / `T \| undefined` |
| RAII: a destructor releases what the object owns | try-with-resources / TS `using`, but it happens at the end of *every* scope, with no keyword |
| `std::unique_ptr<T>` | The one owner of a heap object; freed when the owner goes away (there is no GC) |
| `T&` (reference) | A non-null object reference that can't be pointed elsewhere |
| `std::span<const std::uint8_t>` | A read-only view over bytes, like a `Uint8Array.subarray()`; no copy |
| `std::jthread` | A `Thread` that is joined automatically when destroyed |
| `namespace hidpp` | A Java package / TS module |
| `.h` / `.cpp` | Declarations / implementation, as in C |

**How RGB lands in the next version (M7).**
- A field in `Profile`.
- `SetLeds(Device&, …)` in `hidpp`.
- A submenu in `Tray`.
- The chosen colors saved under `HKCU\Software\SidetoneOff`.

The worker, detection, retries and transport stay as they are.

### 5.1 HID++ layer
- **`hidpp.h/.cpp`** are portable: no Windows headers, everything in `namespace hidpp`. They hold `Report` (20 bytes), `Error`, the `Transport` interface, the `Device` class and the feature functions. They get reused for RGB, EQ, Linux and the Mac backport.
- **`hid_windows.h/.cpp`** hold `WinHidTransport`, the Win32 implementation of `Transport`.

**Request format.** `[0x11, 0xFF, featureIndex, (function << 4) | 0x0A, params…]`, zero-filled to `OutputReportByteLength`. Software ID `0x0A` makes the set packet byte-identical to the spec's `11 FF 07 1A 00`.

**Responses.** `Device::Call` handles these:
- A match has byte 2 = feature index and byte 3 = function/software ID.
- A HID++ error looks like `[0x11, 0xFF, 0xFF, featureIndex, fnSw, errorCode]`.
- Unsolicited reports are skipped.

**`Device`.**
- **`Call(featureId, function, params)`** looks up the feature index, writes the request through the `Transport`, and reads until the matching response arrives or a 1 s deadline passes.
- **The feature index** comes from `IRoot.getFeature`. A `Device` lives for one attempt, so the index is asked once per attempt (spec change #4) and cached for the rest of that attempt. A result of 0 means the headset lacks the feature, and the call fails.

**`WinHidTransport`.**
- **`Open()`**, a static factory:
  1. `HidD_GetHidGuid` + `CM_Get_Device_Interface_List`, keeping the paths that contain `vid_046d&pid_0a89`.
  2. Open each path with access 0 and call `HidP_GetCaps`. Pick UsagePage `0xFF43` / Usage `0x0202`.
     It must also have in and out report lengths of at least 20 bytes, a HID++ long report.
  3. Reopen it with `GENERIC_READ|GENERIC_WRITE` and `FILE_FLAG_OVERLAPPED`.
  4. `HidD_FlushQueue`.
- **`Write`:** overlapped `WriteFile` with a 1 s timeout, and `CancelIoEx` on timeout.
- **`Read`:** overlapped `ReadFile` until the deadline.
- **Its handle is closed** when the object goes out of scope (RAII), so every attempt closes everything, even on an error path.

**One attempt.**
1. `WinHidTransport::Open()`.
2. `hidpp::Device device{transport}`.
3. `Apply(device, profile)`. For sidetone: `SetSidetone(device, 0)`, then `GetSidetone(device)`. **The attempt succeeds only if the read-back is 0.** A timeout or a non-zero value is a failure (M2).

In Phase 2, `--diag` calls `GetSidetone` / `SetSidetone` / `GetSidetone` directly. `Profile` and `Apply` come in Phase 3.

**Result type.** Every fallible step returns `std::expected<T, Error>` (C++23): the value, or an `Error` with the step, the OS error code (`GetLastError` on Windows) and any HID++ error code. `--diag` and the log print the `Error` as is.

**Retries.** Up to 5 attempts, with every failure retried.

**Logging.** `OutputDebugStringW("SidetoneOff: …")` with the paths, caps, feature index, bytes written, level read, `GetLastError` and HID++ error codes.

### 5.2 Detection, worker, state

**Ownership (A1).** `App` owns the hidden window and the state, plus a `DeviceWatcher`, a `Worker` and a `Tray`. Destroying `App` unregisters the notifications, stops the worker and removes the tray icon, in that order.

**Window.** A hidden top-level window with class `SidetoneOff.Window`. Install and uninstall find the running instance by this class. Its window proc forwards each message to an `App` member function (the pointer is stored with `SetWindowLongPtrW(GWLP_USERDATA)`).

**Device notifications.** `DeviceWatcher` calls `RegisterDeviceNotification` twice: for `GUID_DEVINTERFACE_USB_DEVICE` and for the HID GUID.

**Triggers.**
- **Connected state.** USB arrival/removal matching `VID_046D&PID_0A89` (case-insensitive) is the only source of the connected/disconnected state.
  - An arrival sets it.
  - A removal re-reads it from the system: is any G635 USB device still present? This replaces the Mac's +1/−1 counter, so the state can't drift if a notification is missed.
- **USB arrival:** state `applying`, then `RequestApply()`.
- **HID arrival** matching the VID/PID:
  - `RequestApply()`. All 3 collections fire, and coalescing merges them.
  - If the last run failed, the state goes back to `applying`. A first plug-in can outlast the 5 attempts while Windows installs the driver; the HID arrival then starts a fresh run.
- **Startup:** check the present USB interfaces (register first, then check). If there are any, `applying` and `RequestApply()`.
- **Resume** (`PBT_APMRESUMEAUTOMATIC`) while connected: `applying` and `RequestApply()`.
- **"Set sidetone off again":** `applying` and `RequestApply()`.

**Closing while the menu or a dialog is open.** The menu runs inside `Tray::PopUpMenu`, and the Uninstall dialog inside `App::Uninstall`, further up the call stack. So `WM_CLOSE` (taskkill, install, uninstall) can't destroy the window right then. It closes the menu (`EndMenu`) or the dialog (`WM_CLOSE`, the same as Cancel), and posts itself again. A click on the tray icon while the dialog is open brings the dialog to the front instead of opening a second menu.
- `taskkill` without `/f` sends its `WM_CLOSE` to the visible dialog only, so it cancels the dialog and the app keeps running. Install and uninstall post to the hidden window, which closes both.

**Worker.** The `Worker` class owns one thread (`std::jthread`), an auto-reset event `wake`, and a manual-reset event `stop`:
```
loop:
  wait(stop | wake, INFINITE)
  for attempt in 1..5:
      wait(stop | wake, 1000) // the Mac's 1 s wait; a HID arrival (or any new request) cuts it short
      if Apply(profile) on a freshly opened WinHidTransport succeeds: break
  PostMessage(hwnd, WM_APP_RESULT, ok)
```
- A request that arrives during an attempt leaves `wake` signaled, so one extra run follows. That is harmless.
- `stop` ends the loop at any wait. The `Worker` destructor sets it, and the `jthread` joins.
- In this version the profile is fixed: `Profile{ .sidetone = 0 }`.

**Result.** On `WM_APP_RESULT`: if `connectedCount == 0`, ignore it (Mac guard). Otherwise the state becomes `sidetoneOff` or `failed`.

States, menu, tooltip and icon dimming follow the spec's table.

### 5.3 Self-install (replaces the spec's Installer section)

**Locations.**
- **Install dir:** `%LOCALAPPDATA%\Programs\SidetoneOff\SidetoneOff.exe` (`FOLDERID_UserProgramFiles`).
- **Run value:** `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, value `SidetoneOff` = `"<install dir>\SidetoneOff.exe" --autostart` (M14).
- **Start Menu shortcut:** `FOLDERID_Programs\SidetoneOff.lnk`, created with `IShellLinkW` (`ole32`, `uuid`).
- **Uninstall entry:** `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\SidetoneOff`, which makes it show in Settings › Apps › Installed apps. Values:
  - `DisplayName` "SidetoneOff", `DisplayIcon` (`<exe>,0`), `DisplayVersion`, `Publisher` "Giancarlo Dalle Mole" (M5)
  - `InstallLocation`, `InstallDate`, `EstimatedSize`
  - `UninstallString` = `"<exe>" --uninstall`
  - `NoModify=1`, `NoRepair=1`
- **App settings:** `HKCU\Software\SidetoneOff`. Empty in this version (the next one keeps its RGB colors there), but uninstalling deletes it.
- **Startup switch:** when you turn the app off in Settings › Apps › Startup, Windows stores that as the `SidetoneOff` value under `HKCU\…\Explorer\StartupApproved\Run`. Uninstalling deletes it too.

**"Installed" means the same file.** Whether this process runs the installed exe is decided by file identity (volume serial and file ID), not by comparing path strings, so case, short names and links don't matter.

**Entry points.**

| Launch | Behavior |
|---|---|
| No args, exe **outside** the install dir | Confirmation dialog (M3): "Install SidetoneOff?" with **Install** / Cancel, or "Update SidetoneOff to x.y.z?" with **Update** / Cancel when already installed. Yes → install steps; an error shows in a dialog. Cancel → exit. |
| `--install` | Install steps with no dialog. Errors are only logged, with exit code 1. Used by `build.ps1 -Install`, like `build.sh`. |
| No args, exe **inside** the install dir | Tray app: mutex, tray, and the "SidetoneOff is running" notice (M14) |
| `--autostart`, exe **inside** the install dir | The Run value, at sign-in: the tray app, without the notice. From anywhere else it counts as unknown arguments. |
| `--uninstall` | From Settings › Apps. **No dialog** (M4), except to show an error. Uninstall steps. |
| Tray › Uninstall… | Spec's confirmation dialog, default Cancel. Uninstall steps 2–3 run in the app, and the app ends. Once its worker has stopped and the tray icon is gone, `wWinMain` runs step 4. |
| `--diag` | Section 5.5 |
| Any other arguments | Logged, then exit code 2, with no UI |

**Install steps.**
1. Close the running instance: find its window, `PostMessage(WM_CLOSE)`, wait up to 5 s on its process, and `TerminateProcess` if it is still there.
2. Create the dir and copy the exe. Skip the copy if this exe is already the installed one. A copy that finds the old exe still in use is retried for up to 2 s.
3. Write the Run value, the shortcut and the Uninstall key. If a step fails, the steps before it stay done, and installing again finishes the job.
4. Launch the installed exe.
5. Exit.

**Uninstall steps.**
1. Close the running instance, unless we *are* that instance (tray uninstall).
2. Delete the Run value, the StartupApproved value, the Uninstall key, `HKCU\Software\SidetoneOff` and the shortcut. Anything already gone is fine.
3. (The tray uninstall ends the app here.)
4. Delete the files:
   - If this process *is* the installed exe, it can't delete itself. It spawns a hidden `cmd.exe /d /c "ping -n 3 127.0.0.1 >nul & del /f /q "<exe>" & rd "<dir>""`, with full paths to `cmd` and `ping` and the system folder as its working folder, then exits.
   - Otherwise (`--uninstall` from another copy) it deletes them directly.
   - `rd` without `/s` only removes an **empty** folder.
   - No alternate-data-stream self-delete tricks.

**Risk.** An unsigned exe that copies itself, adds a Run value and spawns `cmd` to delete itself can trip Defender heuristics.
- **Mitigations:** `VERSIONINFO`, the confirmation dialog, and plain APIs only. Defender stays on during every test.
- **SmartScreen:** local builds have no Mark-of-the-Web, so no SmartScreen. A downloaded unsigned exe would get the "Windows protected your PC" warning.

### 5.4 Tray, icon, first run
As in the spec, with these specifics:
- **Tray icon:** `Shell_NotifyIconW` with `NOTIFYICON_VERSION_4`, identified by `uID`, not `NIF_GUID` (which ties the icon to the exe path). Re-added on `TaskbarCreated`.
  - After an Explorer restart it comes back wherever Windows keeps it: the ^ overflow until you pin it.
  - Phase 3 uses the stock `IDI_APPLICATION` icon as a placeholder.
- **Menu:** the same menu on a left click (`NIN_SELECT`), a right click (`WM_CONTEXTMENU`) and the keyboard (`NIN_KEYSELECT`). It is built from the current state each time it opens.
  - The status line is always disabled: it shows the state and can't be clicked, as on the Mac (section 3, #22).
  - From Phase 6, "Show log" sits above Uninstall… and Quit (5.7).
  - **Live, like the Mac menu.** If the state changes while the menu is open, it closes and reopens in place with the new state. An open Win32 menu can't be resized, and the status line changes length.
- **Glyph (M12):** an original headphones drawing: a rounded headband (a half circle continued straight down) with filled, rounded ear cups. It's laid out on a 24-unit grid with a 2-unit stroke (M8), and defined once, in `make-icon.ps1`.
- **Tray icon** (`tray_icon.*`):
  - `make-icon.ps1` draws the glyph white on transparent into `build/tray.ico`, at the small-icon size of each display scale from 100% to 400% (16, 20, 24, 28, 32, 36, 40, 48, 56, 64 px). It is embedded as `IDI_TRAY`.
  - Size: `GetSystemMetricsForDpi(SM_CXSMICON, dpi)`, with the DPI of the taskbar window (`Shell_TrayWnd`), which is that of its monitor. Without Explorer, the system DPI. `LoadImageW` picks that image, or scales the nearest one.
  - Only the glyph's alpha is kept. It becomes the alpha of a 32-bpp `HICON` with **straight alpha** (section 3, #16).
  - Color follows `SystemUsesLightTheme`: white on a dark taskbar, black on a light one. Disconnected shows it at 40% opacity.
  - The normal and dimmed icons are made once and kept. They're remade only if the icon size or the theme really changed, which is checked on any `WM_SETTINGCHANGE`, `WM_DPICHANGED`, `WM_DISPLAYCHANGE` and `TaskbarCreated`. Explorer also sends `TaskbarCreated` when the taskbar's DPI changes.
  - If that fails (logged), the stock application icon keeps the tray icon, and its menu, there.
- **Tooltip (M13):** two lines, "SidetoneOff" and the status.
  - Every `Shell_NotifyIconW` call sends the icon, the tooltip and `NIF_SHOWTIP`. With `NOTIFYICON_VERSION_4`, Windows hides the standard tooltip without that flag.
  - The tooltip is sent again right after `NIM_SETVERSION`: the one sent with `NIM_ADD`, before the version was set, doesn't count. Without this, it only appeared after the first status change (found in the Phase 5 review).
- **App icon:** `make-icon.ps1` → `build/app.ico`, embedded in the exe as its first icon (`IDI_APP` in `res/resource.h`). Sizes 16, 20, 24, 32, 48, 64 and 256.
  - The spec's design, as on the Mac, at every size (M11): a tile filling 80% of the canvas, the gradient `#665CF2` → `#3D2EB2`, and the white glyph at about half the tile.
  - 48, 64 and 256 px are stored as PNG, the others as 32-bit bitmaps with a 1-bit mask (section 3, #17). `tray.ico` is all PNG. The script writes the `.ico` files itself, because System.Drawing can't save a multi-size icon with alpha.
- **"SidetoneOff is running" notice:** the spec's first-run text, split into a title, "SidetoneOff is running", and the rest as its text (section 3, #18).
  - Shown with `NIF_INFO` and the app icon (`LoadIconMetric`, large). Windows 11 shows it as a toast; the tray glyph would be white on white there.
  - Shown on every start except the one at sign-in (`--autostart`) (M14).
- **Dialogs** are `TaskDialogIndirect`, titled "SidetoneOff": install and update with the app icon; uninstall with the warning icon, Cancel as default; errors with the error icon.
  - Every one closes with Esc, Alt+F4 or `WM_CLOSE` (`TDF_ALLOW_DIALOG_CANCELLATION`).
  - An error names the failed step and Windows' reason, e.g. "Couldn't copy the app into its folder: Access is denied. (error 5)".

### 5.5 `--diag`
Runs without the mutex or tray, so it can run while the tray instance is running. It makes one attempt with no retries. The report lists:
- the USB interface(s);
- every HID collection for 046D/0A89, with path, usage page and usage, and in/out/feature lengths, marking the one picked;
- HID++ version, firmware, and the index of `0x8300`;
- the level read **before**, the write result (bytes, or `GetLastError` + `FormatMessageW`), and the level read **after**;
- the **HID++ traffic**: every report sent and received, in hex. A `RecordingTransport` wraps the real transport to capture it;
- a note: "Sidetone is only audible while an app uses the mic (for example, with Sound > Recording open)." It uses ASCII `>` so the text survives any console code page.

**Output.**
- **Default:** a `MessageBoxW`. Ctrl+C copies its text.
- **`--diag --console`:** writes to stdout with no dialog, so scripts and agents can use it.
  - Into a pipe or a file, it writes UTF-8. With no redirection, it attaches to the parent's console.
  - If there is no console at all, it falls back to the dialog.
- **From PowerShell:** the exe is a GUI-subsystem app, so PowerShell only waits for it, and sets `$LASTEXITCODE`, when its output is piped:
  ```
  .\SidetoneOff.exe --diag --console | Out-Host
  ```

**Exit code.** 0 when the after-level reads 0, otherwise 1.

### 5.6 Layout and build
```
.gitignore  .gitattributes
mac/                   Mac app (unchanged)
thoughts/              spec + plan
windows/
  build.ps1            build (+ -Install); fail fast
  make-icon.ps1        the glyph (M12) → build/app.ico and build/tray.ico, via System.Drawing
  README.md
  src/
    main.cpp           wWinMain, arg dispatch, mutex
    app.cpp/.h         App: hidden window, window proc, state machine
    hidpp.cpp/.h       portable HID++ 2.0: Transport interface, Device, feature functions
    profile.cpp/.h     portable: Profile (what to apply on connect) and Apply()
    hid_windows.cpp/.h WinHidTransport: find the G635 collection, overlapped I/O
    win32.h            RAII wrappers for Win32 handles
    worker.cpp/.h      Worker: thread, coalescing, retries
    devices.cpp/.h     DeviceWatcher: notifications, enumeration, counting
    tray.cpp/.h        Tray: the icon, menu, balloon
    tray_icon.cpp/.h   the tray icon: taskbar size and theme, the glyph from tray.ico recolored
    install.cpp/.h     install / update / uninstall (free functions)
    diag.cpp/.h        --diag (free function)
    strings.h          every UI string in one place (backport / Linux)
    version.h          the single version number (code + rc)
    log.cpp/.h         Log(): OutputDebugString ("SidetoneOff: …" lines) and the history
    log_history.cpp/.h portable: LogHistory, the last 500 lines (Phase 6)
    log_window.cpp/.h  LogWindow: Tray > Show log (Phase 6)
  res/
    app.rc  app.manifest  resource.h
  tests/
    tests.cpp          FakeTransport + checks for hidpp (and profile, from Phase 3; log_history, from Phase 6)
```

**`build.ps1`.**
1. Find VS with `vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`. Build Tools are only found with `-products *`.
2. Prepend the VS Installer dir to PATH. Without it, VsDevCmd prints "'vswhere.exe' is not recognized".
3. `Enter-VsDevShell -arch=x64`.
4. Run `make-icon.ps1`, which writes `build/app.ico`, then `rc` with `/I build`, so `app.rc` finds the icon.
5. Fail fast if `$env:VCToolsVersion` is below **14.52**, the first MSVC with stable `/std:c++23`.
6. Compile: `cl /std:c++23 /permissive- /utf-8 /W4 /WX /sdl /MT /O1 /GL /Zi /EHsc /guard:cf`, with `UNICODE`, `WIN32_LEAN_AND_MEAN`, `NOMINMAX` and `_WIN32_WINNT=0x0A00` defined. `/utf-8` keeps the "—" in the status strings intact.
7. Link: `link /WX /LTCG /SUBSYSTEM:WINDOWS /DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /PDBALTPATH:%_PDB% /GUARD:CF /CETCOMPAT /MANIFEST:EMBED /MANIFESTUAC:NO /MANIFESTINPUT:res\app.manifest` with `kernel32 user32 gdi32 shell32 comctl32 advapi32 ole32 uuid hid cfgmgr32`.
   - The PDB stays in `build/`, and the exe points to it by file name only, not by your local path.
   - The manifest is the only source of the UAC level.
   - **Unit tests (M10)** run before this link:
     - Compile `tests/tests.cpp` with the same flags.
     - Link it with the portable objects (`$TestedSources`: `hidpp` now, `profile` from Phase 3, `log_history` from Phase 6) into the console exe `build/SidetoneOffTests.exe`.
     - Run it. A failure stops the build, so no new `SidetoneOff.exe` is produced.
8. Check `$LASTEXITCODE` after every tool.
9. Print the exe size, and fail if it is over 300 KB.
10. With `-Install`, run `build\SidetoneOff.exe --install`. This runs after the environment is restored, so the installed app doesn't start inside the build tools' environment. It waits for that one process (`Process.WaitForExit`), not with `Start-Process -Wait`, which also waits for the tray app it starts.

Every build starts from an empty `build/`. If the exe is running from it, the build stops with a clear message. When it finishes, the script restores the caller's environment, because `Enter-VsDevShell` changes it and repeated runs in the same terminal would otherwise pile up.

Output goes to `windows/build/`, which git ignores.

### 5.7 Log window (Phase 6)
**History (M15, M16).**
- Every `Log()` line also goes, after its date and time (`2026-11-23 14:02:12  `), into an in-memory `LogHistory` of the last 500 lines.
- Each process keeps its own, and only the tray app's is shown. It covers the time since the app started, and there are still no log files.
- `LogHistory` is portable (no Windows headers) and thread-safe, because the worker thread logs too. It numbers the lines, so a reader can ask for those added since the last one it saw. Unit tests cover the 500-line cap and reading from a cursor.

**Window.**
- Tray › **Show log** opens it. Choosing it again brings the open window to the front.
- A resizable top-level window titled "SidetoneOff — Log", with the app icon. Its whole client area is a read-only, word-wrapped text box (a multiline EDIT control) with a vertical scroll bar, in Consolas 10 pt on a white background (M17).
- Sized for its monitor's DPI when it opens, and resized, with a new font, on `WM_DPICHANGED`.
- **Live:** each new line posts a message to the window, which appends the lines since its cursor.
  - It follows new lines only if it was scrolled to the end, and keeps the reader's selection.
  - The window can stay open for days. Once its text box holds 1000 lines, it is refilled with the history, which keeps the last 500. That bounds its memory too.
- **Keys:** Esc closes it, Ctrl+A selects all, and Ctrl+C or the right-click menu copies.
- Closing destroys the window, but the history stays.
- `App` owns the `LogWindow`. Quit, an update or an uninstall closes it with the app.

**Limits.**
- Install, update and uninstall run in a separate short-lived process, so their steps aren't in the window. The new app's history starts at "started, version …".
- `--diag` keeps its own one-time report.

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
- `main.cpp`: mutex `Local\SidetoneOff.SingleInstance`, the hidden window, the message loop, `WM_CLOSE` → exit. Plain functions for now; they move into the `App` class in Phase 3, when state appears.
- `log.h`.
- No tray and no icon yet.
- Plan: code structure (A1), English (L1), the reviewer notes (section 2), and open questions M9 and M10. All of these were decided after the skeleton was built.

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
**Changes**
- `hidpp.*`: `Transport`, `Device`, `GetSidetone` / `SetSidetone`.
- `hid_windows.*`: `WinHidTransport`.
- `win32.h`: RAII wrappers. `main.cpp`'s mutex becomes a `UniqueHandle`, which removes its three manual `CloseHandle` calls.
- `diag.*`, and the `--diag` dispatch in `main.cpp`. Any other arguments exit with code 2.
- `tests/tests.cpp` and the test step in `build.ps1` (M10).

**My checks**
- `--diag --console` with the G635 plugged in should show Col03 picked, out 20, index 7, after-level 0, and exit code 0.
- `--diag` also works while the tray instance is running.
- The tests pass, and a deliberately broken check fails the build with no exe left behind.

**You**
1. Unplug → I run `--diag` → it exits 1 with "G635 not found".
2. Replug → I run it → before **40**, after **0**.
3. Run `.\windows\build\SidetoneOff.exe --diag` with no `--console` → MessageBox, and Ctrl+C copies the text. A double-click can't pass the argument.

If anything fails here, it gets fixed before Phase 3 (spec).

**Commit title.** `Phase 2: Talk HID++ to the G635 and add --diag`

### Phase 3: Detection, worker, tray menu
**Changes**
- `app.*`: the `App` class, which takes over the window, the window proc and the state machine. `main.cpp` keeps `wWinMain`, the argument dispatch and the mutex.
- `profile.*`: `Profile` and `Apply()`. Unit tests for `Apply`, including a read-back that isn't 0, and `profile` added to `$TestedSources`.
- `devices.*` (`DeviceWatcher`), `worker.*` (`Worker`), and `tray.*` (`Tray`) with a stock placeholder icon.
- Resume handling and `TaskbarCreated`.
- Menu: status line / Set sidetone off again / Quit. Uninstall… comes in Phase 5. The menu updates live while open (5.4).
- `WM_CLOSE` while the menu is open is deferred until the menu has closed (5.2).

**My checks**
- Capture the app's `OutputDebugString` lines with a scratch listener.
- With the headset connected at startup, the log shows the send and the read-back of 0, and `--diag --console` confirms 0.
- Post `TaskbarCreated` and a resume to the window → the icon is re-added, and sidetone is applied again.
- Open the menu by posting the tray callback, change the state while it's open, and screenshot it → the menu reopens with the new state.
- `WM_CLOSE` while the menu is open → clean exit.
- Idle memory and CPU.

**You**
1. Unplug and replug, with Sound › Recording open. The status should go disconnected → applying → sidetone off, and the sidetone should be gone within about 2 s.
2. Use "Set sidetone off again".
3. Use Quit.
4. Run `taskkill /f /im explorer.exe; start explorer` → the icon comes back (look in the ^ overflow too).
5. Sleep and resume → sidetone off.
6. With the menu open, unplug → the menu switches to "disconnected" without closing.

**Commit title.** `Phase 3: Detect the headset and turn sidetone off from the tray`

### Phase 4: Icons
**Changes**
- `tray_icon.*`: the tray glyph, for the taskbar's theme and DPI.
- `Tray` keeps a normal and a dimmed icon, shows the dimmed one while disconnected, and redraws both when the taskbar changes. `App` passes on `WM_SETTINGCHANGE`, `WM_DPICHANGED` and `WM_DISPLAYCHANGE`.
- `win32.h`: RAII wrappers for icons, GDI objects and memory DCs.
- `make-icon.ps1` → `app.ico` with sizes 16/20/24/32/48/64/256, the glyph thickened (M8), and the small-size layout (M11). It's embedded via `rc` (`res/resource.h`).
- Plan: M11, section 3 #15–#17, and 5.4.
- (The Phase 5 review replaced the Segoe glyph with a drawn one (M12) and reverted the small-size layout (M11).)

**My checks**
- The exe's icon group has every size, and `LoadImage` loads each one at its exact size.
- I render preview PNGs of the tray icon (light/dark, connected/dimmed, 100–200%) and of the app icon, and look at them.
- `DrawIconEx` draws the tray icon's edges as straight alpha predicts.
- Sending the app a settings change, a display change and `TaskbarCreated` redraws nothing when nothing changed, and the app keeps running.

**You**
1. Switch Windows between light and dark mode → the icon color follows. The icon may be in the ^ overflow until you pin it.
2. Set display scale to 150% → the icon stays sharp.
3. Unplug → the icon dims.
4. The exe's icon in Explorer.

**Commit title.** `Phase 4: Draw the tray and app icons`

### Phase 5: Self-install, update, uninstall, first run
**Changes**
- `install.*`: the install and uninstall steps, and the dialogs.
- The `--install` / `--uninstall` dispatch, and the confirmation dialog for an exe run from outside the install dir (M3).
- Tray › Uninstall… with its dialog. `WM_CLOSE` while it is open closes it first (5.2).
- The first-run notice.
- `build.ps1 -Install`.
- `win32.h`: `UniqueHkey`. `strings.h`: the new UI text and the install names.
- Fixes from your review (M11–M14):
  - The drawn glyph, in `tray.ico` and the app icon.
  - The app icon back to the Mac layout.
  - The tooltip on two lines, shown from the start.
  - "Logitech G635 connected".
  - The notice on every start but sign-in (`--autostart`).
  - The menu's status line always disabled.

**My checks**
- `build.ps1 -Install` → the folder, Run value (with `--autostart`), shortcut and Uninstall key (Publisher M5) exist, the app runs from the install folder, and the notice was shown.
- An update while the app is running → the old PID exits and a new PID runs, and shows the notice.
- Quit, then a start with no arguments shows the notice; a start with `--autostart` doesn't; `--autostart` from another copy exits with code 2.
- Tray and app icon previews with the drawn glyph, and the icon group in the exe.
- A second launch of the installed exe exits at once.
- The update dialog and the Uninstall dialog, captured as images. Enter in the Uninstall dialog cancels.
- `WM_CLOSE` to the app while the Uninstall dialog is open → the dialog closes, and the app exits.
- Tray › Uninstall… → Uninstall → the registration is gone at once, the exe and folder about 2 s later.
- `--uninstall` as Settings runs it, while the app is running → all of them are gone, with no dialog (M4).
- `--uninstall` from another copy also removes a StartupApproved value, and deletes the files directly.
- Defender's detection history stays empty.

**You**
1. Double-click install → confirmation dialog → the notice shows once.
2. Installed apps shows the entry with the icon and "Giancarlo Dalle Mole", and Startup apps lists it.
3. Tray › Uninstall… → Enter cancels, the button confirms.
4. Settings › Apps › Uninstall while the app is running.
5. Defender › Protection history stays clean.

**Commit title.** `Phase 5: Install, update and uninstall without an installer`

### Phase 6: Log window
Added 2026-11-23, after Phase 5, at your request. Design in 5.7.

**Changes**
- `log_history.*` (portable): `LogHistory`, with unit tests, and added to `$TestedSources`.
- `log.h` / `log.cpp`: `Log()` also records the line, with its time, and tells the log window.
- `log_window.*`: `LogWindow`.
- Tray › Show log. `App` owns the window.
- Plan: M15–M17, section 3 #23, 5.7.

**My checks**
- The history's unit tests pass.
- Open the window through the menu (by window messages) and capture it → the lines since the start, with their times.
- A new line while it is open (e.g. a simulated resume) appears at the end.
- Esc closes it. Quit, and an update (`--install`), with the window open → a clean exit.
- Memory and CPU at idle after the window was opened and closed. Five open/close cycles leave the handle, GDI and USER object counts flat.

**You**
1. Tray › Show log → the history. Scroll, select, copy.
2. Unplug and replug while it is open → the new lines appear.
3. Resize it; if you have a second monitor with another scale, move it there.
4. Esc closes it.

**Commit title.** `Phase 6: Show the app's log in a window`

### Phase 7: README and release check
**Changes**
- `windows/README.md`: build, install, uninstall, `--diag`, the log window, the mic-in-use note, troubleshooting.
- Update the plan's status.

**My checks**
- Section 7 end to end, starting from a clean state.
- Working set under 5 MB (`Get-Process`).
- CPU time doesn't grow over 60 s idle.

**You.** The hardware items of section 7.

**Commit title.** `Phase 7: Document the Windows app and record the release check`

---

## 7. Release checklist (run in Phase 7; the phase where each item first appears is in brackets)

1. `build.ps1` from a clean clone → `build\SidetoneOff.exe`, no warnings, all unit tests pass, < 300 KB. [P1, P2]
2. `--diag` with the G635 → Col03 / `0xFF43`/`0x0202`, out 20, index 7, after-level 0, exit 0. [P2]
3. Double-click the exe → confirmation dialog → tray icon and the "SidetoneOff is running" notice. Quit and reopen → the notice again; it doesn't show at sign-in. The app shows in Installed apps (icon, Publisher) and in Startup apps. [P5]
4. Plug / unplug → the status and icon change. After plugging in, `--diag` reads 0, and with Recording open there's no sidetone. [P3]
5. Restart Windows with the headset connected → sidetone off. [P5]
6. Sleep → resume while connected → sidetone off. [P3]
7. Restart Explorer → the tray icon comes back. [P3]
8. A second launch from the Start Menu → nothing happens. [P1/P5]
9. Run a newer build while the app is running → update dialog, the old instance closes, the new one runs. [P5]
10. Task Manager: < 5 MB, 0% CPU idle. [P7]
11. Tray › Uninstall… (Enter = Cancel) → confirm → nothing left: folder, Run value, Uninstall key, `HKCU\Software\SidetoneOff`, shortcut. [P5]
12. Settings › Apps › Uninstall while running → same result, no extra dialog. [P5]
13. Defender on throughout → nothing in Protection history. [P5/P7]
14. Tray › Show log → the history since the app started, with times. Unplug and replug while it is open → the new lines appear. [P6]

### Release check of 1.0.0 (2026-11-23)

My checks ran on the 1.0.0 build, on this PC, with the G635 connected and Defender on. They were run again, all passing, after the rename to "SidetoneOff" (M18). Items marked *yours* need hands, eyes or a reboot.

| # | My result | Yours |
|---|---|---|
| 1 | **Pass.** A fresh `git clone`, with the uncommitted changes copied in, builds with 0 warnings, 20/20 unit tests and a 268 KB exe. | — |
| 2 | **Pass.** `--diag --console`: Col03, `FF43`/`0202`, out 20, protocol 4.2, firmware U1 30.00, index 7, level after 0, exit 0. | — |
| 3 | **Pass.** No-argument start → "Install SidetoneOff?" → Install → the app runs from the install folder and shows its notice. The Uninstall key holds 1.0.0 and the Publisher; the Run value ends in `--autostart`. Task Manager › Startup apps lists it as "SidetoneOff.exe" with no publisher, like every other classic Run entry on this PC: Edge, Windows Security, AMD, Figma and IDrive, all signed and with proper descriptions. Only Store apps get names there. For a while before the restart it showed "SidetoneOff" and the publisher, and IDrive showed "IDrive"; the restart cleared both. That is Windows' doing; the exe's version info is correct. | *Yours:* see it in Installed apps and Startup apps. |
| 4 | — | *Yours:* plug and unplug, with Sound › Recording open. |
| 5 | **Pass** (you restarted at 16:32). The app's own history: started at sign-in with `--autostart` at 16:33:26, sidetone off on attempt 1 one second later, and no notice. | — |
| 6 | **Pass (simulated).** A posted resume message → sidetone off on attempt 1. | *Yours:* a real sleep and wake. |
| 7 | **Pass (simulated, Phase 3–4).** `TaskbarCreated` → the icon is added again. | *Yours:* restart Explorer. |
| 8 | **Pass.** A second launch of the installed exe exits at once with code 0; one instance remains. | — |
| 9 | **Pass.** No-argument start while running → "Update SidetoneOff to 1.0.0?" → Update → the old process exits cleanly, a new one starts. | — |
| 10 | **Pass.** Task Manager's Memory (private working set) 1.5 MB; CPU time unchanged over 60 s idle. The full working set is 11.8 MB, mostly shared pages of Windows' own DLLs. | — |
| 11 | **Pass.** Enter in the Uninstall dialog cancels; the Uninstall button removes everything, the files within 5 s. | — |
| 12 | **Pass.** The `UninstallString`, run as Settings › Apps runs it, while the app runs → nothing left, no dialog. | *Yours:* the real Settings › Apps › Uninstall. |
| 13 | **Pass.** `Get-MpThreatDetection`: 0 detections today, across every install, update and uninstall of Phases 5–7. | *Yours:* a look at Protection history. |
| 14 | **Pass.** Show log lists the 1.0.0 history with times, including the simulated resume. | *Yours:* unplug and replug while it's open. |

---

## 8. Future (next versions)

- **RGB** via `0x8070` ColorLEDEffects. Not in this version (M7); it's the next one.
  - The G635 lists the feature, and HeadsetControl already drives its two zones, logo and side strips.
  - It is probably volatile too, so the same re-apply-on-connect path would cover it.
  - The UI would be a submenu of presets plus `ChooseColorW`.
  - With A1 it lands as a `Profile` field, `SetLeds()` and a `Tray` submenu (5.0).
- **Equalizer** via `0x8310`.
- **Other headsets in the family:** G633 `0A5C`, G933 `0A5B`, G935 `0A87`, G733 `0AB5`/`0AFE`/`0B1F`. HeadsetControl uses the same protocol for all of them; it's a PID table, but untested here.
- **Backport to Mac:**
  - native HID (IOKit `IOHIDDevice`) instead of `headsetcontrol`. The same `Transport` / `Device` shape maps to a Swift protocol and a class;
  - read-back;
  - resume trigger;
  - HID-arrival trigger;
  - the app icon with the drawn glyph (M12). Apple's terms don't allow SF Symbols in app icons, even on the Mac. The menu-bar icon can stay SF Symbols;
  - the display name "SidetoneOff" (M18): `CFBundleName` and `CFBundleDisplayName` in `Info.plist`, the bundle `SidetoneOff.app`, and the strings in `main.swift`.
- **Linux:** reuse `hidpp.*` and `profile.*`, with a `HidrawTransport` (`hidraw` + udev monitor), StatusNotifierItem for the tray, and the drawn glyph (M12), which is ours and works on any platform.
