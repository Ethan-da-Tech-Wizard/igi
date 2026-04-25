# Product Requirements Document (PRD): Igi

## 1. Product Overview
**Igi** is a context-aware, minimalist visual search utility for macOS. It acts as a universal "Ctrl+F" for unsearchable environments, allowing users to find and highlight text within scanned documents, complex UI dashboards, and remote desktop streams. It operates entirely on the visual layer, prioritizing high-performance latency and strict security constraints.

## 2. Core Features & Requirements
* **Global Accessibility:** The tool must be triggerable via a global macOS hotkey or by launching the application.
* **Active Context Capture:** It must seamlessly capture the visual state of the currently active window or user-defined screen region.
* **Instantaneous OCR Engine:** Visual data must be processed into searchable text arrays with near-zero latency.
* **Minimalist Overlay UI:** A dark-mode, floating search bar that does not obstruct the user's workflow. It must draw a physical highlight polygon over the matched word directly on the screen.
* **Zero-Storage Architecture (HIPAA Compliant):** The tool must process all visual and text data strictly in volatile RAM. It is explicitly prohibited from writing caches, temporary files, or transmitting data to the internet. All memory is purged upon closing the search.

## 3. Technology Stack
* **Language:** C++17
* **Build System:** CMake (Version 3.16+)
* **UI & Capture Framework:** Qt 6 (Core, Gui, Widgets)
* **OCR Engine:** Tesseract OCR (with Leptonica for image processing)
* **PDF Rendering:** MuPDF (selected for high-performance, direct file-path memory loading when the active window is a PDF reader)

## 4. System & Development Requirements
To successfully build and run Igi natively on macOS, the following environment specifications are required:

### Operating System
* **macOS 13 (Ventura) or newer.** 

### Compiler & Build Tools
* **Xcode Command Line Tools:** Version 15.2 or newer is strictly required. Apple Clang must be properly linked via `xcode-select`. Older versions of Xcode tools will cause C++ macro failures (`crau_data` bug) when compiling required visual libraries from source.

### Package Management (Homebrew)
The following dependencies must be installed via Homebrew:
1. `cmake` (Build automation)
2. `qt` (Cross-platform UI framework and screen capture API)
3. `tesseract` (Optical Character Recognition engine)
4. `leptonica` (Image processing capabilities required by Tesseract)
5. `mupdf` (Lightweight PDF engine)

**Installation Command:**
```bash
brew install cmake qt tesseract leptonica mupdf
```

## 5. Security & Privacy Constraints
* **No Network Requests:** The application must remain completely air-gapped from network services.
* **No Disk I/O:** The application must not create `.tmp` files. All Qt Pixmaps and Tesseract text arrays must be allocated and destroyed within standard RAM lifecycles.
