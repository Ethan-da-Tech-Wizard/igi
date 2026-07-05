# Igi: Cross-Platform Compatibility Plan

**Status:** Proposed (D-008) · **Date:** 2026-07-05 · **Companion doc:** `docs/CROSS_PLATFORM_TODO.md`

This document is the research + design plan for taking Igi from a macOS-only
utility to a tool that runs on **macOS, Windows, and Linux** without breaking
any of them. It is **additive**: nothing that exists today is removed or
rewritten. The macOS implementation stays exactly as shipped — it becomes one
of three first-class platform backends behind the interfaces that already
exist in the codebase.

> **Prime directive:** macOS must never regress. Every phase in the TODO ends
> with the existing `macos-14` CI job still green. All new platform code lives
> behind `#if` guards, CMake `if(APPLE)/if(WIN32)/if(UNIX)` blocks, and the
> factory interfaces (`IHotkeyListener::create()`, `IScreenCapture::create()`)
> that were designed for exactly this.

---

## 1. Where we start (research summary)

### 1.1 What is already cross-platform (keep as-is, zero changes)

The architecture was quietly built for this. These components have **no**
macOS dependency and compile anywhere Qt 6 + Tesseract + Leptonica exist:

| Component | Files | Notes |
|-----------|-------|-------|
| Fuzzy search (D-001) | `src/search/FuzzySearch.{h,cpp}`, `src/search/BoundingBox.h` | Pure C++17, Qt-free |
| OCR engine | `src/ocr/OcrEngine.{h,cpp}` | Tesseract C++ API, portable |
| Image conversion | `src/ocr/ImageConverter.{h,cpp}` | QImage → Leptonica PIX, portable |
| Screen geometry / DPR math | `src/core/ScreenGeometry.{h,cpp}` | Pure Qt, portable |
| Search session controller | `src/SearchSession.{h,cpp}` | Pure Qt, portable |
| Daemon shell | `src/core/Daemon.{h,cpp}` | Already takes `IHotkeyListener` via DI |
| Search bar UI | `src/ui/SearchOverlay.{h,cpp}` | Frameless Qt widget, portable |
| Capture flash overlay | `src/ui/CaptureFlashOverlay.{h,cpp}` | Portable |
| Secure wipe | `src/core/SecureWipe.h` | Volatile-loop `igi_bzero` already has a portable fallback branch |
| System tray | `main.cpp` (`QSystemTrayIcon`) | Qt handles macOS menu bar / Windows tray / Linux SNI |
| Test suite | `tests/*` (GoogleTest, D-005) | Runs anywhere with `QT_QPA_PLATFORM=offscreen` |

### 1.2 What is macOS-only today (keep, but add siblings)

Six Objective-C++ translation units, all correctly fenced behind `if(APPLE)`
in `CMakeLists.txt` already:

| macOS backend | macOS API used | Stays? |
|---------------|----------------|--------|
| `HotkeyListener.mm` | `NSEvent` global monitor (D-004), Carbon fallback | ✅ untouched |
| `ScreenCapture.mm` | `CGWindowListCreateImage` | ✅ untouched |
| `ActiveWindowInspector.mm` | AX API (`kAXFocusedWindowAttribute`, `kAXDocumentAttribute`) | ✅ untouched |
| `Permissions.mm` | `CGPreflightScreenCaptureAccess`, `AXIsProcessTrusted` (TCC) | ✅ untouched |
| `AppPolicy.mm` | `NSApplicationActivationPolicyAccessory` | ✅ untouched |
| `SecurityGuard.mm` | DYLD injection check, Hardened Runtime, codesign, FileVault | ✅ untouched |

### 1.3 Portability hazards found in the current tree (must fix carefully)

These are the specific places that would break a Windows or Linux build
today. None require changing macOS behaviour — only adding guards/siblings:

1. **`src/main.cpp`** includes `<unistd.h>` and uses the POSIX self-pipe
   trick (`pipe()`, `sigaction`). Fine on Linux as-is; Windows needs a
   `SetConsoleCtrlHandler` / `WM_QUERYENDSESSION` equivalent behind
   `#ifdef Q_OS_WIN`.
2. **`src/main.cpp`** loads the tray icon from
   `applicationDirPath() + "/../Resources/AppIcon.icns"` — a macOS bundle
   path, and `.icns` doesn't load outside macOS. Needs a Qt resource
   (`.qrc`) PNG fallback (additive; the `.icns` path stays for macOS).
3. **`ActiveWindowInspector.h`** uses `pid_t` — not defined by MSVC; needs a
   portable typedef (`qint64` or `#ifdef` alias).
4. **`CMakeLists.txt`** finds Tesseract/Leptonica via `pkg-config` only —
   fine on Linux, needs vcpkg/`find_package` route on Windows. tessdata
   lookup assumes Homebrew paths.
5. **`scripts/verify_deps.py` + `deps.lock`** are Homebrew-specific (T-1
   supply-chain check). Needs per-platform lock sections (apt/vcpkg), not a
   rewrite.
6. **`scripts/sandbox_test.sh` + `resources/igi.sb`** use `sandbox-exec`
   (macOS-only). D-007 write-deny enforcement needs per-OS equivalents
   (see §4.3).
7. **CI** (`.github/workflows/ci.yml`) runs only on `macos-14`. Needs a
   matrix — as **new jobs**, never edits that risk the existing one.
8. **Docs inconsistency (pre-existing):** hotkey is `Cmd+Shift+F` in
   docs/headers but `Cmd+Shift+9` in `main.cpp`'s tray label/log. Resolve as
   part of the per-platform hotkey table (§3.1) without deleting history.

---

## 2. Target architecture: one interface, three backends

No redesign is needed — the factory pattern in `HotkeyListener.h` /
`ScreenCapture.h` is extended, and two headers gain the same treatment
(`Permissions`, `ActiveWindowInspector`, `SecurityGuard`).

```
                      ┌──────────────────────────────┐
                      │   Shared C++/Qt core (§1.1)  │
                      │  Daemon · SearchSession · UI │
                      │  OCR · FuzzySearch · Wipe    │
                      └──────────────┬───────────────┘
                                     │ factory interfaces
        ┌────────────────────────────┼────────────────────────────┐
        ▼                            ▼                            ▼
┌────────────────┐         ┌──────────────────┐         ┌──────────────────────┐
│ macOS backend  │         │ Windows backend  │         │ Linux backend        │
│ (existing .mm, │         │ (new .cpp, WIN32)│         │ (new .cpp, X11 +     │
│  UNCHANGED)    │         │                  │         │  Wayland portals)    │
└────────────────┘         └──────────────────┘         └──────────────────────┘
```

Proposed file layout (new files only; nothing moves):

```
src/core/
  HotkeyListener.mm          # existing macOS  (unchanged)
  HotkeyListener_win.cpp     # new: RegisterHotKey
  HotkeyListener_linux.cpp   # new: XGrabKey (X11) / portal GlobalShortcuts (Wayland)
  ScreenCapture.mm           # existing macOS  (unchanged)
  ScreenCapture_win.cpp      # new: Windows.Graphics.Capture → BitBlt fallback
  ScreenCapture_linux.cpp    # new: XComposite/XGetImage (X11) / portal Screenshot (Wayland)
  ActiveWindowInspector.mm   # existing macOS  (unchanged)
  ActiveWindowInspector_win.cpp    # new: GetForegroundWindow + UIAutomation
  ActiveWindowInspector_linux.cpp  # new: _NET_ACTIVE_WINDOW (X11); degraded on Wayland
  Permissions.mm             # existing macOS  (unchanged)
  Permissions_stub.cpp       # new: Win/Linux — reports "granted" or portal state
  SecurityGuard.mm           # existing macOS  (unchanged)
  SecurityGuard_win.cpp      # new: DLL injection / Authenticode / BitLocker advisory
  SecurityGuard_linux.cpp    # new: LD_PRELOAD check / LUKS advisory
```

### 2.1 Per-OS API mapping (the core research)

| Capability | macOS (shipped) | Windows (planned) | Linux X11 (planned) | Linux Wayland (planned) |
|---|---|---|---|---|
| Global hotkey | `NSEvent` global monitor, Carbon fallback (D-004) | `RegisterHotKey()` Win32 | `XGrabKey()` | XDG portal `GlobalShortcuts` (user approves once) |
| Focused-window capture | `CGWindowListCreateImage` | `Windows.Graphics.Capture` (Win10 1903+); `PrintWindow`/`BitBlt` fallback | `XComposite`/`XGetImage` | XDG portal `Screenshot`/`ScreenCast` (consent dialog) |
| Active window info | AX API | `GetForegroundWindow` + `DwmGetWindowAttribute` (true bounds) + UIAutomation (doc path, best-effort) | `_NET_ACTIVE_WINDOW` + `_NET_WM_NAME` | Not exposed by design → capture the full active output and OCR it all (graceful degradation, documented) |
| Permissions model | TCC: Screen Recording + Accessibility preflight | None required (consent border drawn by OS for Graphics.Capture) | None (X11) | Portal consent dialogs replace preflight |
| Secure wipe (D-006) | `igi_bzero` volatile loop | Same loop (also `SecureZeroMemory` available) | Same loop (`explicit_bzero` in glibc ≥ 2.25) | same |
| Memory locking | `mlock()` | `VirtualLock()` | `mlock()` (check `RLIMIT_MEMLOCK`) | same |
| Signal-safe teardown (T-2) | POSIX self-pipe (shipped) | `SetConsoleCtrlHandler` + `WM_QUERYENDSESSION` | POSIX self-pipe (works as-is) | same |
| Hide from dock/taskbar | `LSUIElement` + activation policy | `Qt::Tool` window flags, no main window (already true) | no `.desktop` `NoDisplay` needed; tray only | same |
| Tray icon | `QSystemTrayIcon` (shipped) | `QSystemTrayIcon` | `QSystemTrayIcon` (StatusNotifier; GNOME may need AppIndicator ext. — document) | same |
| Injection check (SEC-GUARD-01) | `DYLD_INSERT_LIBRARIES` | `AppInit_DLLs` / suspicious loaded modules | `LD_PRELOAD` | same |
| Disk-encryption advisory | FileVault (SEC-GUARD-06) | BitLocker status (soft) | LUKS/dm-crypt heuristic (soft) | same |
| Write-deny test (D-007) | `sandbox-exec` (shipped) | AppContainer / restricted-token run (best-effort; documented gap otherwise) | `bwrap --ro-bind /` (bubblewrap) | same |

### 2.2 Hotkey per platform

`Cmd+Shift+F` has meaning only on macOS. Proposal (single table, referenced
everywhere instead of hard-coded strings):

| OS | Default hotkey |
|----|----------------|
| macOS | `⌘⇧F` (as shipped — whatever current binding is stays; the F/9 doc mismatch is reconciled here, not by touching macOS code paths blindly) |
| Windows | `Ctrl+Shift+F` |
| Linux | `Ctrl+Shift+F` (portal lets the user rebind on Wayland) |

A `PlatformHotkey.h` constant + one Qt tray-label lookup replaces the two
hard-coded strings in `main.cpp` (additive refactor, macOS string preserved).

### 2.3 What gracefully degrades (so the app never breaks)

The rule is: **a missing platform capability downgrades the feature, never
crashes the app.** Specifically:

- **Wayland, no active-window API** → capture entire active monitor via the
  portal and OCR the whole screen. Search still works; highlights are
  screen-anchored instead of window-anchored.
- **Wayland, portal denied/missing** → tray icon shows "capture unavailable";
  hotkey shows a friendly dialog (mirrors the existing macOS
  permissions-dialog pattern in `main.cpp` — reuse it).
- **Windows < 10 1903 (no Graphics.Capture)** → `BitBlt` fallback path.
- **PDF direct-ingest (Chunk 6 / MuPDF)** → MuPDF is portable, but the
  "which file is open" AX trick is macOS-only. Windows: UIAutomation
  best-effort. Linux: skip → screen-capture fallback (the fallback is already
  the designed behaviour in CHUNKSTONES Chunk 6).
- **Hotkey registration fails anywhere** → existing `Daemon` already treats
  this as non-fatal (Chunk 1 DoD: "registration failure does not crash the
  daemon"). Same contract on all OSes.

---

## 3. Distribution: three one-file downloads, or build it yourself

Per the product direction: a user should land on the README and see exactly
**four ways to get Igi — three single-file downloads (one per OS) or a
from-source build.** The macOS artifact is what ships today; the other two are
what this plan adds.

### Option 1 — macOS: `Igi.dmg` *(exists today — unchanged)*
- Built by `bundle.sh` / CI, Developer ID signed + notarized (Chunk 7).
- Universal Binary, macOS 13+.
- Download → drag to Applications → grant two permissions → done.

### Option 2 — Windows: `IgiSetup-x64.exe` *(new)*
- Single-file Inno Setup installer (or portable single `Igi.exe` as a
  stretch goal), built with `windeployqt` + statically-bundled
  tesseract/leptonica via vcpkg, `eng.traineddata` embedded alongside.
- Authenticode-signed when a cert secret is configured (mirrors the
  conditional Apple-cert logic already in `ci.yml`).
- Windows 10 1903+ (for Graphics.Capture; older gets BitBlt fallback).

### Option 3 — Linux: `Igi-x86_64.AppImage` *(new)*
- Single self-contained file: Qt, tesseract, leptonica, tessdata all inside;
  `chmod +x && ./Igi-x86_64.AppImage` and it runs. No root, no package
  manager, matches the zero-footprint ethos.
- Built with `linuxdeploy` + Qt plugin on the oldest supported base
  (Ubuntu 22.04 LTS) for maximum glibc compatibility.
- Flatpak considered and deferred: sandbox portals actually help Igi, but
  AppImage is the true "download one file" experience. Revisit post-launch.

### Option 4 — Build it yourself *(all OSes)*
- One documented path per OS in a new `docs/BUILDING.md`:
  - **macOS:** Homebrew deps + `cmake` (current flow, documented as-is).
  - **Linux:** `apt install qt6-base-dev libtesseract-dev libleptonica-dev` + `cmake`.
  - **Windows:** vcpkg manifest (`vcpkg.json`) + Visual Studio 2022 + `cmake --preset windows`.
- `CMakePresets.json` added so every OS is `cmake --preset <os> && cmake --build`.

---

## 4. Keeping the guarantees on every OS

The security posture (README §5, THREAT_MODEL, D-006/D-007) is part of the
product. Each guarantee maps per-OS:

### 4.1 Zero-write (D-007)
The read/write boundary is enforced in code identically (no platform
difference — the code simply never writes). The *verification* differs:
macOS `sandbox-exec` (shipped) · Linux `bwrap` read-only root · Windows
best-effort restricted token, with the gap explicitly documented in
`RISK_REGISTER.md` if full parity isn't reachable.

### 4.2 Volatile memory purge (D-006)
`igi_bzero`'s volatile loop is already portable. `mlock` → `VirtualLock` on
Windows behind one small wrapper. RSS-return-to-baseline test (D-003) runs in
all CI legs.

### 4.3 Offline / no-network
Nothing platform-specific — the "no network frameworks linked" check
(`otool -L`) gets siblings: `ldd` on Linux, `dumpbin /dependents` on Windows.

### 4.4 Latency SLOs (D-002)
D-002 numbers were calibrated on Apple Silicon. Windows/Linux get their own
measured baselines during Phase 3/4 benchmarking **added as new rows** to
DECISIONS.md — macOS budgets are not touched.

---

## 5. CI / release matrix

Existing two macOS jobs stay byte-identical. New jobs are appended:

| Job | Runner | What it proves |
|-----|--------|----------------|
| `build-and-test` *(existing)* | `macos-14` | macOS never regresses |
| `bundle-sign-sandbox` *(existing)* | `macos-14` | DMG + D-007 |
| `linux-build-and-test` *(new)* | `ubuntu-22.04` | Compiles + unit tests pass (`QT_QPA_PLATFORM=offscreen`) |
| `linux-appimage` *(new)* | `ubuntu-22.04` | Single-file AppImage builds + bwrap write-deny test |
| `windows-build-and-test` *(new)* | `windows-2022` | MSVC compiles + unit tests pass |
| `windows-installer` *(new)* | `windows-2022` | `IgiSetup-x64.exe` builds |

The Linux compile job is deliberately **Phase 0** (before any Linux feature
code): it locks in "the shared core compiles everywhere" and catches
portability regressions from day one.

---

## 6. Risks specific to this effort

| ID | Risk | Mitigation |
|----|------|-----------|
| XP-01 | Wayland can't identify/capture the focused window silently | Portal consent + full-screen OCR degradation (§2.3); document UX difference |
| XP-02 | Platform `#ifdef` creep breaks macOS build subtly | All platform code in separate TUs selected by CMake, not inline `#ifdef` soup; macOS CI gate on every PR |
| XP-03 | Windows Defender flags an unsigned screen-capturing hotkey app | Authenticode signing + SmartScreen reputation building; document it |
| XP-04 | GNOME hides `QSystemTrayIcon` by default | Document AppIndicator extension; app still fully usable via hotkey |
| XP-05 | tessdata pathing diverges per platform | Single `TessdataLocator` helper with per-OS search order + `TESSDATA_PREFIX` override (already supported) |
| XP-06 | Homebrew-only `deps.lock` gives false security confidence on new platforms | Extend lockfile schema with per-platform sections (apt pin + sha256, vcpkg baseline) |
| XP-07 | AppImage on musl/imm. distros | State glibc ≥ 2.35 floor; source build covers the rest |

---

## 7. Phase overview (details in CROSS_PLATFORM_TODO.md)

| Phase | Deliverable | Breaks macOS? |
|-------|-------------|---------------|
| 0 | Portability groundwork: Linux CI compile job, CMake guards, `pid_t`/tray-icon/main.cpp fixes, presets | No — gated by existing CI |
| 1 | Linux X11 backend (hotkey, capture, active window) | No — new files only |
| 2 | Linux Wayland via XDG portals + graceful degradation | No |
| 3 | Windows backend (hotkey, capture, active window, ctrl-handler) | No |
| 4 | Packaging: AppImage + Inno Setup installer + `BUILDING.md` (the "3 downloads or build it" story) | No |
| 5 | Security parity: per-OS SecurityGuard, write-deny tests, dep locks | No |
| 6 | Docs/QA sweep: per-OS QA matrix, SLO baselines, RISK_REGISTER/THREAT_MODEL addenda | No |

Estimated effort: ~5–7 weeks single engineer (Linux X11 is the cheapest win;
Wayland and Windows packaging carry the unknowns).

---

*This plan is additive to `docs/SCOPE_AND_MILESTONES.md` (whose original
scope lock is preserved and superseded by dated note, not deletion) and is
recorded as decision D-008 in `docs/DECISIONS.md`.*
