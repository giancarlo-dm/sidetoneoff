# Sidetone Off — Windows 1.0.0 — Session summary (2026-11-21)

This is a record of the first working session on the Windows port: what was learned, decided and built, and where to pick up. It doesn't repeat the plan. Decisions and design are in [plan.md](plan.md), which wins over [spec.md](spec.md) where they differ.

**Where things stand:** Phases 0–3 are committed. Phase 4 (icons) is next. One open question remains: M9, the release version.

---

## 1. Research before the plan

**Inputs.**
- The spec (`spec.md`).
- The Mac app (`mac/main.swift`), which turns sidetone off by shelling out to `headsetcontrol -s 0`.

**Toolchain on this PC.**

| | Start of the session | After the upgrade |
|---|---|---|
| Build tools | VS 2019 Build Tools, MSVC 14.29 (stops at C++20) | **VS 2026 Build Tools 18.12, MSVC 14.52** (`/std:c++23`), via `winget`. VS 2019 removed afterwards. |
| Windows SDK | | 10.0.26100 |

- **Quirks:**
  - `vswhere` only finds Build Tools with `-products *`.
  - VsDevCmd needs the VS Installer dir on PATH.
- winget asked for a reboot to finish the VS 2026 install. Builds work regardless; reboot if you haven't yet.

**G635 probing (throwaway probes, real headset).**
- **Collections.** Three HID collections under `MI_03`. HID++ goes over **Col03**: usage page `0xFF43`, usage `0x0202`, 20-byte input and output reports.
- **HID++ 2.0.** Protocol 4.2, firmware `U1 30.00 build 0007`. Features: Root, FeatureSet, FwVersion, DeviceName, **ColorLEDEffects 0x8070**, GKey, Equalizer 0x8310, **Sidetone 0x8300 at index 7** (fn0 get, fn1 set, 0–100).
- **Packet.** The set packet `11 FF 07 1A 00 …` works without admin rights.
- **Audibility.** Sidetone is only audible while some app captures the mic. The first listening test was a false negative until Sound › Recording was open.
- **No onboard memory.** After a power cycle the level reads 40, the default. That's why the app must re-apply on every connection. An unexplained 40 earlier was traced to an unclean reboot (Kernel-Power 41).
- **RGB.** Possible via 0x8070, but deferred to the next version (M7).

## 2. Decisions taken in this session

All are in plan.md §1. The ones that shape the work most:

**Installer and install behavior**
- **Self-installing exe** instead of Inno Setup (spec change #1).
- **Confirmation dialogs:** confirm on install (M3); no second dialog on `--uninstall` (M4).
- **Publisher** "Giancarlo Dalle Mole" (M5).

**Device behavior**
- **Read-back verification:** an attempt succeeds only when the headset reads back 0. A read-back timeout is a failure (M2).
- **Triggers:** HID-collection arrival triggers a send, in addition to USB arrival and resume.

**UI**
- **Quit** only quits.
- **App icon:** the glyph is thickened with a stroke (M8).

**Code and toolchain**
- **Toolchain:** C++23 (T1) on VS 2026 Build Tools (T2).
- **A1, hybrid code structure.**
  - Classes for what owns a resource or state: `App`, `Worker`, `DeviceWatcher`, `Tray`, `hidpp::Device`, `WinHidTransport`.
  - Free functions for operations: `SetSidetone`, later `SetLeds`.
  - One `Transport` interface; no hierarchies.
  - Errors are `std::expected` values.
  - Chosen with RGB and other future features in mind.
- **M10, unit tests:** yes, with a `FakeTransport`, run on every build.

**Working agreements**
- One commit per phase, reviewed by you. Open questions come first in the plan, as Blocker or Minor blocker; once answered they become "Decisions" and the plan is reconciled.
- You always commit by hand. I hand over a title (≤ 80 chars) and a description without hard wraps. VS Code's subject-length warning was raised to 80.
- The conversation is in English (L1). You review as a Java/TypeScript developer, so the code stays plain C++, and each phase report has "C++ notes".

**Open:** M9, the release version. Currently 0.1.0; the recommendation is 1.0.0 at Phase 6.

## 3. Phases done

| Phase | Commit | What | Verified |
|---|---|---|---|
| 0 | a99b5ad | Repo at the workspace root: Mac app, spec, plan, `.gitignore`, `.gitattributes` | — |
| 1 | 862538a | `build.ps1` (clean build, `/W4 /WX`, LTCG, CFG, CET, embedded manifest, 300 KB cap, restores the caller's environment), skeleton (mutex, hidden window), `VERSIONINFO`, manifest, `log.h`. The plan was amended with A1 and L1. | 133 KB; imports only system DLLs; second instance exits in ~5 ms; 0.7 MB memory |
| 2 | bebc4c0 | `hidpp.*` (portable HID++), `hid_windows.*` (`WinHidTransport`, overlapped I/O with timeouts), `win32.h` (RAII), `--diag` / `--diag --console` with the full HID++ traffic, 11 unit tests run by the build | On the G635: level 40 before, 0 after, exit 0; unplugged gives "G635 not found", exit 1 |
| 3 | f2cf924 | `App`, `Worker` (thread, 5 × 1 s retries, request merging), `DeviceWatcher`, `Tray` (tooltip and menu, placeholder icon), `Profile` / `Apply` with read-back, 16 tests | Startup, unplug/replug, Set again, Quit, Explorer restart, resume; 214 KB; 1.1 MB and 0 CPU idle |

## 4. Found during review and testing

- **Stale open menu (Phase 3, found by you).** Unplugging while the menu was open left it showing the old state. Win32 menus don't update or resize while open.
  - Fix: if the state changes while the menu is open, it closes and reopens in place. This matches the Mac's live menu. Verified with screenshots.
- **Close during the menu (Phase 3, found while fixing the above).** A `WM_CLOSE` with the menu open would have destroyed the `Tray` under the running menu. Phase 5's install and update send exactly this.
  - Fix: close the menu, then re-post `WM_CLOSE`.
- **Explorer restart.** The icon does come back, in the ^ overflow, where Windows keeps unpinned icons. The first report of "it didn't come back" came from looking only at the taskbar.
- **Replug timing.** The first attempt can fail with write error 31 while the headset is still coming up. The retry succeeds on attempt 2, which is what the retries are for.
- **Connected state.** After each removal it's re-read from the system instead of kept as the Mac's counter, so it can't drift (spec change #13).
- **Collection lengths.** Picking the HID++ collection requires 20-byte reports (the plan said ≥ 5).

## 5. Practical notes

- **Build:** `.\windows\build.ps1`, in Windows PowerShell 5.1 or 7. It always builds from an empty `build/`, so stop `SidetoneOff.exe` first if it runs from there.
- **Diagnose:** `.\windows\build\SidetoneOff.exe --diag --console | Out-Host`. The exe is a GUI-subsystem app, so PowerShell only waits for it, and sets `$LASTEXITCODE`, when the output is piped.
- **Logs:** `OutputDebugString` lines prefixed `Sidetone Off:`. Use DebugView or any DBWIN listener.
- **Scratch tools:** a DBWIN listener (`dbwin.exe`), `poke.ps1` (posts TaskbarCreated or a resume, prints memory and CPU), `menutest.ps1` (opens the menu, changes state, takes screenshots), `inspect.ps1` (dumpbin, manifest, version) and the HID probes. They lived in the session scratchpad and aren't part of the repo.
- **Measuring:** Windows performance counters (WMI, PDH) are broken on this PC; memory is measured with `K32GetProcessMemoryInfo`.
- **Lesson for hardware tests:** start the log listener right before asking for hardware steps, give it a long window, and check that the log covers them. One round was lost because the listener had stopped.

## 6. Next

1. **Phase 4, icons.**
   - The tray glyph (Segoe Fluent `U+E7F6`) is rendered for the theme and DPI, and dimmed when disconnected.
   - `make-icon.ps1` builds the app `.ico` (gradient tile, thickened glyph), embedded via `rc`.
2. **Phase 5, self-install.** Install, update, uninstall, the first-run balloon, Tray › Uninstall…, and `build.ps1 -Install`.
3. **Phase 6.** README, the release checklist (plan §7), and answer M9.
4. **Later (plan §8).** RGB via 0x8070, EQ via 0x8310, other headset models (a PID table), backport to the Mac (native IOKit HID, read-back, resume and HID triggers), a Linux port reusing `hidpp` and `profile`.
