# ⚡ Igi OCR (High-Performance Visual Search)

**The Problem:** Scanned documents, complex images, and PDFs with stylized characters cannot be searched using the standard `Ctrl+F` function. 

**The project:** Igi explores visual search using C++, screen capture, OCR and fuzzy matching. Accuracy, latency and document coverage depend on the input and have not been independently verified. 

## 🎯 Core Capabilities

To be perfectly clear on what this tool is and what it does:

1. **Low-Latency Fuzzy Search:** Built from the ground up in C++ to keep the search loop under target SLOs (p50 < 800ms, p95 < 1500ms — see `docs/DECISIONS.md`). The matcher is lexical fuzzy search (Levenshtein), not embedding-based semantic search.
2. **Screen search design:** The project describes a background utility that captures visible screen content; platform support and performance require testing.
3. **Highlighting:** The design includes highlighting OCR matches. Detection and positioning are not guaranteed.
4. **Offline operation:** The project is designed for local OCR. Offline and network behavior across configurations have not been independently verified.
5. **Data handling:** The project describes in-memory screen processing. Disk writes, logs, data retention, and network behavior have not been independently verified; do not rely on a no-write or complete-erasure guarantee.

## 🚀 How It Works

Igi operates as a pure utility. It does not try to be a clunky document manager. 

1. You have an unsearchable 70-page scanned PDF open, or a heavily stylized website.
2. You activate Igi via a global hotkey (`Cmd+Shift+9` on macOS, `Ctrl+Shift+9` on Windows and Linux).
3. The project is designed to capture visible content. Direct PDF handling is configuration-dependent; see the CMake options and project source.
4. You type your search term in the minimalist floating search bar.
5. Igi finds the closest fuzzy match (forgiving common OCR errors) and draws a highlight over the word directly on your screen. 
6. Closing a search ends the session; complete memory erasure and absence of stored data have not been verified.

## 🛠 Technology Stack

This tool requires raw power and memory control. It is built using:
- **C++ (Qt Framework):** For the lightning-fast, cross-platform user interface, global hotkey detection, and floating search bar overlay.
- **C++ (libtesseract):** The raw, native OCR engine for high-performance text extraction.
- **C++ (Leptonica):** Holds active-window screen captures directly in RAM, without touching the hard drive.
- **PDF handling and licenses:** The current CMake configuration detects optional Poppler. Historical design notes also discuss MuPDF, but its use and current PDF behavior require verification. PDF library licensing must be reviewed for any distribution.

## 📦 Get Igi — one file per OS, or build it yourself

Igi began life macOS-only. The macOS app is unchanged and remains the
reference implementation; Windows and Linux support is being added around it
(see `docs/CROSS_PLATFORM_PLAN.md` and decision D-008). The goal: however you
run it, getting Igi is **one single-file download** — or one build command.

### 1. 🍎 macOS — `Igi.dmg` *(historical release notes)*
- Historical notes describe a signed and notarized universal DMG for macOS 13+. Current availability, signatures and notarization have not been verified.
- Download → drag **Igi** to Applications → launch → grant the two
  permissions it asks for (Screen Recording + Accessibility). Done.

### 2. 🪟 Windows — `IgiSetup-x64.exe` *(planned — Phase 3/4 of the port)*
- Single-file installer with everything bundled (Qt, Tesseract, language
  data). Windows 10 (1903) or later.
- Download → run the installer → Igi lives in your system tray. Hotkey:
  `Ctrl+Shift+9`.

### 3. 🐧 Linux — `Igi-x86_64.AppImage` *(planned — Phase 1/2/4 of the port)*
- One self-contained file, no root and no package manager needed:
  `chmod +x Igi-x86_64.AppImage && ./Igi-x86_64.AppImage`
- Planned X11 and Wayland behavior is described as follows; it has not been independently verified. On Wayland, capture goes through the desktop portal
  (the system asks your consent once) and search runs against the whole
  active screen.

### 4. 🛠 Build it yourself *(all platforms)*
The short version (a fuller `docs/BUILDING.md` is planned):

| OS | Dependencies | Build |
|----|--------------|-------|
| macOS | `brew install cmake qt tesseract leptonica pkg-config` | `cmake -B build && cmake --build build` |
| Arch Linux | `pacman -S cmake qt6-base tesseract tesseract-data-eng leptonica libx11 gtest pkgconf` | `cmake -B build && cmake --build build` |
| Ubuntu / Debian | `apt install cmake qt6-base-dev libtesseract-dev libleptonica-dev libx11-dev libgtest-dev libgmock-dev pkg-config` | `cmake -B build && cmake --build build` |
| Windows *(untested)* | Visual Studio 2022 + vcpkg (`vcpkg.json` manifest) | `cmake --preset windows && cmake --build build` |

On a headless machine (CI, containers), set `QT_QPA_PLATFORM=offscreen` before building, because the test binary runs once during the build to discover tests. Run the suite with `ctest --test-dir build`. To skip tests and their GoogleTest dependency, configure with `-DIGI_BUILD_TESTS=OFF`.

> **Viewport search:** The project describes OCR for visible screen content. Hotkeys and capture behavior vary by platform and should be tested.

> Platform behavior and privacy properties have not been independently verified. Capture support, data handling, failure behavior, and permissions may differ by system.

## License

Igi is released under the [MIT License](LICENSE). It links against Qt 6 (LGPL-3.0), Tesseract (Apache-2.0) and Leptonica (BSD-2-Clause), which keep their own licenses.
