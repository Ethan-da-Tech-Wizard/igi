# Igi: Cross-Platform TODO / Plan of Action

**Status:** Proposed (D-008) · **Date:** 2026-07-05 · **Plan:** `docs/CROSS_PLATFORM_PLAN.md`

Working checklist for the cross-platform effort. Rules of the road:

- ☑️ = done, ⬜ = open. Check items off; **never delete them** (this doc is
  append/update-only, like the rest of `docs/`).
- **Every phase has the same exit gate:** existing macOS CI jobs
  (`build-and-test`, `bundle-sign-sandbox`) pass unchanged. If a task can't
  be done without touching macOS code paths, it goes back to design review.
- New platform code = new files behind CMake platform blocks. Existing `.mm`
  files and macOS behaviour are read-only for this effort.

---

## Phase 0 — Portability groundwork (no user-visible features)

*Goal: the shared core compiles on Linux and Windows in CI before any
platform feature work starts.*

- ⬜ 0.1 Add `linux-build-and-test` CI job (`ubuntu-22.04`): apt deps
  (`qt6-base-dev`, `libtesseract-dev`, `libleptonica-dev`), configure with
  `-DIGI_BUILD_TESTS=ON`, run `ctest` with `QT_QPA_PLATFORM=offscreen`.
  Append as a new job — zero edits to the two existing macOS jobs.
- ⬜ 0.2 Fix `pid_t` in `src/core/ActiveWindowInspector.h` with a portable
  alias (keep macOS semantics identical).
- ⬜ 0.3 Guard POSIX-only code in `src/main.cpp` (`<unistd.h>`, self-pipe,
  `sigaction`) behind `#if !defined(Q_OS_WIN)`; leave macOS/Linux path
  byte-identical. Stub the Windows branch (real impl in Phase 3).
- ⬜ 0.4 Tray icon: add a PNG to a Qt resource file as the cross-platform
  icon; keep the existing `.icns` bundle path as the macOS branch.
- ⬜ 0.5 Introduce `src/core/PlatformHotkey.h` (per-OS default-hotkey
  constants + display string). Replace the two hard-coded strings in
  `main.cpp`; reconcile the pre-existing `Cmd+Shift+F` vs `Cmd+Shift+9`
  doc/code mismatch here (documented, nothing deleted).
- ⬜ 0.6 CMake: add `if(WIN32)` / `if(UNIX AND NOT APPLE)` source blocks
  (initially empty/stub backends that return `nullptr` + log), plus
  `CMakePresets.json` with `macos`, `linux`, `windows` presets.
- ⬜ 0.7 Stub factories: `Permissions`, `SecurityGuard`,
  `ActiveWindowInspector` get header-level factory/`#if` split so non-Apple
  builds link (stubs report "granted"/no-op with a warning log).
- ⬜ 0.8 Add `TessdataLocator` helper: per-OS tessdata search order
  (bundle → `TESSDATA_PREFIX` → system paths). macOS order unchanged.
- ⬜ 0.9 Verify `deps.lock` schema extension design (per-platform sections);
  Linux/apt section lands with 0.1, Windows/vcpkg with Phase 3.

**Gate:** Linux CI job green (compile + unit tests) · macOS CI green.

---

## Phase 1 — Linux backend, X11 first

- ⬜ 1.1 `HotkeyListener_linux.cpp`: `XGrabKey` implementation of
  `IHotkeyListener` (default `Ctrl+Shift+F`).
- ⬜ 1.2 `ScreenCapture_linux.cpp`: focused-window capture via
  XComposite/`XGetImage`, DPR-aware (reuse `ScreenGeometry`).
- ⬜ 1.3 `ActiveWindowInspector_linux.cpp`: `_NET_ACTIVE_WINDOW` +
  `_NET_WM_NAME` + window geometry.
- ⬜ 1.4 Wire `Permissions` stub: X11 needs no TCC-style preflight — return
  granted; keep the dialog plumbing for Phase 2 portals.
- ⬜ 1.5 Unit tests: mock-driven tests for the new backends mirroring
  `hotkey_listener_test.cpp` patterns; X11 integration test under Xvfb in CI.
- ⬜ 1.6 Manual QA pass on stock Ubuntu 22.04 (X11 session): hotkey →
  capture → OCR → search → highlight → Esc teardown; RSS returns to
  baseline (D-003 check).

**Gate:** e2e demo on X11 · Xvfb CI test green · macOS CI green.

---

## Phase 2 — Linux Wayland (portals + graceful degradation)

- ⬜ 2.1 Session-type detection (`XDG_SESSION_TYPE`/`WAYLAND_DISPLAY`) →
  backend selection at the existing factory seams.
- ⬜ 2.2 Hotkey via XDG desktop portal `GlobalShortcuts` (D-Bus); fall back
  to X11 path under XWayland where available.
- ⬜ 2.3 Capture via portal `Screenshot` API; reuse the macOS
  permissions-dialog pattern from `main.cpp` for portal-denied UX.
- ⬜ 2.4 Degradation path: no focused-window info on Wayland → full-active-
  output OCR, screen-anchored highlights (per PLAN §2.3). Must not crash,
  must tell the user what's degraded (tray tooltip).
- ⬜ 2.5 QA on GNOME (Wayland) + KDE Plasma (Wayland); document
  GNOME AppIndicator note (XP-04).

**Gate:** Wayland demo with consent flow · denial path shows dialog, no
crash · macOS + Linux CI green.

---

## Phase 3 — Windows backend

- ⬜ 3.1 Toolchain: `vcpkg.json` manifest (qtbase, tesseract, leptonica),
  `windows` CMake preset, MSVC 2022; extend `deps.lock` with vcpkg baseline.
- ⬜ 3.2 `windows-build-and-test` CI job (`windows-2022`), unit tests
  offscreen.
- ⬜ 3.3 `HotkeyListener_win.cpp`: `RegisterHotKey` (default `Ctrl+Shift+F`).
- ⬜ 3.4 `ScreenCapture_win.cpp`: `Windows.Graphics.Capture` primary,
  `PrintWindow`/`BitBlt` fallback for <1903.
- ⬜ 3.5 `ActiveWindowInspector_win.cpp`: `GetForegroundWindow` +
  `DwmGetWindowAttribute` bounds; UIAutomation document-path best-effort.
- ⬜ 3.6 Teardown parity (T-2): `SetConsoleCtrlHandler` +
  `WM_QUERYENDSESSION` → `session->dismiss()` (fills the 0.3 stub).
- ⬜ 3.7 `VirtualLock` branch in the mlock wrapper; confirm `igi_bzero`
  volatile loop on MSVC (or `SecureZeroMemory`).
- ⬜ 3.8 Manual QA on Windows 10 + 11: full loop, multi-monitor DPI
  (per-monitor DPR echoes the Retina fix in `ScreenGeometry`).

**Gate:** e2e demo on Win10 & Win11 · all three OS CI legs green.

---

## Phase 4 — Packaging: "3 downloads or build it yourself"

- ⬜ 4.1 Linux: `linux-appimage` CI job — `linuxdeploy` + Qt plugin,
  tessdata bundled → single `Igi-x86_64.AppImage` artifact.
- ⬜ 4.2 Windows: `windows-installer` CI job — `windeployqt` + Inno Setup →
  single `IgiSetup-x64.exe`; Authenticode signing conditional on secrets
  (mirror the Apple-cert pattern in `ci.yml`).
- ⬜ 4.3 macOS: no changes — confirm `Igi.dmg` job untouched and still green.
- ⬜ 4.4 Write `docs/BUILDING.md`: one build-from-source recipe per OS
  (Homebrew / apt / vcpkg) + presets usage.
- ⬜ 4.5 Update `README.md` "Get Igi" section: the three single-file
  downloads + build-from-source, with per-OS requirement notes (additive;
  existing content untouched).
- ⬜ 4.6 Release workflow: tag → upload all three artifacts to the GitHub
  Release page.

**Gate:** a user on each OS can download exactly one file and run Igi.

---

## Phase 5 — Security & guarantee parity

- ⬜ 5.1 `SecurityGuard_linux.cpp`: `LD_PRELOAD` check (hard),
  LUKS/dm-crypt advisory (soft).
- ⬜ 5.2 `SecurityGuard_win.cpp`: loaded-module injection check (hard),
  Authenticode self-check (hard when signed), BitLocker advisory (soft).
- ⬜ 5.3 D-007 write-deny test, Linux: `bwrap --ro-bind /` harness in CI
  (sibling of `sandbox_test.sh`).
- ⬜ 5.4 D-007 write-deny test, Windows: restricted-token/AppContainer
  best-effort; if not achievable, record the gap in `RISK_REGISTER.md`.
- ⬜ 5.5 No-network link check per OS: `otool -L` (exists) + `ldd` +
  `dumpbin /dependents` CI steps.
- ⬜ 5.6 Extend `scripts/verify_deps.py` for apt/vcpkg lock sections (T-1
  parity), keeping the Homebrew path exactly as-is.

**Gate:** every README §5 guarantee demonstrably holds (or has a documented,
risk-registered gap) on all three OSes.

---

## Phase 6 — Docs, QA matrix, and SLO baselines

- ⬜ 6.1 Measure and record Windows/Linux latency + memory baselines; append
  per-OS rows to D-002/D-003 (macOS numbers untouched).
- ⬜ 6.2 Append cross-platform sections to `THREAT_MODEL.md` and
  `RISK_REGISTER.md` (XP-01…XP-07 from the plan).
- ⬜ 6.3 QA matrix doc: macOS 13/14/15 · Win 10/11 · Ubuntu X11/Wayland ·
  Fedora Wayland · KDE — full loop + teardown + multi-monitor each.
- ⬜ 6.4 Append dated status updates to `SCOPE_AND_MILESTONES.md` as
  Milestones 7+ (originals preserved).
- ⬜ 6.5 Update `docs/ARCHITECTURE.md` with an additive "Platform backends"
  section + diagram (existing macOS diagrams stay).
- ⬜ 6.6 Final sweep: repo docs mention nothing false about platform support;
  the "No Windows or Linux" scope-lock line carries its supersession note.

**Gate:** docs and reality agree on all three OSes; all CI legs green.

---

## Standing invariants (apply to every task above)

1. macOS `.mm` files, `Info.plist`, entitlements, signing, and DMG flow are
   **read-only** for this effort.
2. Zero-write (D-007), volatile purge (D-006), offline-only: identical
   behaviour on every OS.
3. A missing OS capability degrades the feature with a user-visible
   explanation; it never crashes and never silently no-ops.
4. Docs are updated additively — existing information is never removed or
   overwritten, only appended to or annotated with dated notes.
