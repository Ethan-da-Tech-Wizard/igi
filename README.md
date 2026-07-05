# ⚡ Igi OCR (High-Performance Visual Search)

**The Problem:** Scanned documents, complex images, and PDFs with stylized characters cannot be searched using the standard `Ctrl+F` function. 

**The Solution:** Igi is an ultra-low latency, C++ powered utility that provides visual `Ctrl+F` functionality on **any** document, regardless of how it was scanned or formatted. 

## 🎯 Core Capabilities

To be perfectly clear on what this tool is and what it does:

1. **Low-Latency Fuzzy Search:** Built from the ground up in C++ to keep the search loop under target SLOs (p50 < 800ms, p95 < 1500ms — see `docs/DECISIONS.md`). The matcher is lexical fuzzy search (Levenshtein), not embedding-based semantic search.
2. **Context-Aware Global Utility:** There is no "file uploader" or clunky UI. Igi runs silently in the background and instantly reads the currently active website or open PDF on your screen when summoned.
3. **Universal Highlighting:** It has the precise capacity to read the document and visually "highlight" the exact requested word directly over your active window.
4. **100% Offline Capable:** It does not require an internet connection. The C++ engine is completely self-contained and air-gapped.
5. **Reads, Never Writes:** Igi reads pixel buffers from the screen and (when applicable) reads PDF files the user has already opened. It **never writes** captured visuals or extracted text — no temp files, no caches, no logs of user content, no network calls. All in-memory state is purged when a search session ends.

## 🚀 How It Works

Igi operates as a pure utility. It does not try to be a clunky document manager. 

1. You have an unsearchable 70-page scanned PDF open, or a heavily stylized website.
2. You activate Igi via a global hotkey (e.g., `Cmd+Shift+F`).
3. Igi instantly detects the active window. For websites, it pulls visual data directly from the screen. For PDFs, it seamlessly detects the open file and ingests it into volatile RAM.
4. You type your search term in the minimalist floating search bar.
5. Igi finds the closest fuzzy match (forgiving common OCR errors) and draws a highlight over the word directly on your screen. 
6. When you close the search, the memory is instantly purged. Nothing is left behind.

## 🛠 Technology Stack

This tool requires raw power and memory control. It is built using:
- **C++ (Qt Framework):** For the lightning-fast, cross-platform user interface, global hotkey detection, and floating search bar overlay.
- **C++ (libtesseract):** The raw, native OCR engine for high-performance text extraction.
- **C++ (MuPDF/Leptonica):** For securely ingesting complex PDFs and active-window captures directly into RAM without touching the hard drive. MuPDF is used for its superior speed and lightweight memory footprint.

## 📦 Get Igi — one file per OS, or build it yourself

Igi began life macOS-only. The macOS app is unchanged and remains the
reference implementation; Windows and Linux support is being added around it
(see `docs/CROSS_PLATFORM_PLAN.md` and decision D-008). The goal: however you
run it, getting Igi is **one single-file download** — or one build command.

### 1. 🍎 macOS — `Igi.dmg` *(available now)*
- Signed & notarized DMG, macOS 13+ (Universal Binary).
- Download → drag **Igi** to Applications → launch → grant the two
  permissions it asks for (Screen Recording + Accessibility). Done.

### 2. 🪟 Windows — `IgiSetup-x64.exe` *(planned — Phase 3/4 of the port)*
- Single-file installer with everything bundled (Qt, Tesseract, language
  data). Windows 10 (1903) or later.
- Download → run the installer → Igi lives in your system tray. Hotkey:
  `Ctrl+Shift+F`.

### 3. 🐧 Linux — `Igi-x86_64.AppImage` *(planned — Phase 1/2/4 of the port)*
- One self-contained file, no root and no package manager needed:
  `chmod +x Igi-x86_64.AppImage && ./Igi-x86_64.AppImage`
- X11 fully supported; on Wayland, capture goes through the desktop portal
  (the system asks your consent once) and search runs against the whole
  active screen.

### 4. 🛠 Build it yourself *(all platforms)*
Full per-OS instructions land in `docs/BUILDING.md`; the short version:

| OS | Dependencies | Build |
|----|--------------|-------|
| macOS | `brew install cmake qt tesseract leptonica pkg-config` | `cmake -B build && cmake --build build` |
| Linux | `apt install cmake qt6-base-dev libtesseract-dev libleptonica-dev pkg-config` | `cmake -B build && cmake --build build` |
| Windows | Visual Studio 2022 + vcpkg (`vcpkg.json` manifest) | `cmake --preset windows && cmake --build build` |

> Every platform keeps the same guarantees: 100% offline, zero disk writes,
> volatile-RAM-only, no telemetry. If an OS can't support a capability
> (e.g., focused-window detection on Wayland), Igi degrades that feature
> gracefully and tells you — it never crashes and never quietly stores data.
