# Igi: Comprehensive Risk Register

This document identifies potential technical, security, and operational risks associated with the Igi architecture. Each risk is evaluated by Probability (Low, Medium, High), Impact (Low, Medium, High), and provides a strict mitigation strategy.

---

## 1. Security & Privacy Risks

| ID | Risk Description | Probability | Impact | Mitigation Strategy |
|:---|:---|:---:|:---:|:---|
| **SEC-01** | **macOS Page Swapping (RAM to Disk)** <br> The operating system automatically swaps volatile memory (containing the captured screen pixels) to the physical hard drive (swap file) under heavy memory pressure, violating the zero-storage mandate. | Low | High | Utilize macOS `mlock()` and `madvise()` API calls to explicitly lock the memory pages containing `QPixmap` and Tesseract arrays, preventing the OS from writing them to swap storage. |
| **SEC-02** | **Memory Dump Exploitation** <br> A malicious background process or malware accesses Igi's allocated heap memory before the C++ destructor can purge it. | Low | High | Ensure Igi runs strictly in User Space. Implement instant zeroing (`memset(0)`) of pixel arrays immediately after OCR extraction finishes, minimizing the window of vulnerability to mere milliseconds. |
| **SEC-03** | **Revoked Screen Recording Permissions** <br> The user or enterprise IT policy revokes macOS Screen Recording permissions, completely breaking the Qt capture engine. | Med | High | Implement a startup check utilizing `CGPreflightScreenCaptureAccess()`. If access is denied, spawn a polite Qt dialogue routing the user to System Settings rather than crashing the daemon. |

## 2. Performance & Memory Risks

| ID | Risk Description | Probability | Impact | Mitigation Strategy |
|:---|:---|:---:|:---:|:---|
| **PERF-01** | **Severe C++ Memory Leaks** <br> Destructors fail to fire during an edge-case termination (e.g., force quit), causing raw uncompressed 5K Retina display arrays to bloat RAM indefinitely. | High | High | Implement RAII (Resource Acquisition Is Initialization) design patterns. Use C++ smart pointers (`std::unique_ptr`) for all Leptonica `PIX` structures and Tesseract engine instances to guarantee automatic memory destruction regardless of crash state. |
| **PERF-02** | **UI Freezing (Main Thread Blocking)** <br> Tesseract OCR takes >500ms to parse a highly complex 5K display, causing the macOS beachball and freezing the active window. | High | Med | Decouple the OCR pipeline from the Qt GUI Event Loop. Run `TessBaseAPI::Recognize()` exclusively on a detached, asynchronous C++ worker thread (e.g., `std::thread` or `QtConcurrent`). |

## 3. Engineering & Compatibility Risks

| ID | Risk Description | Probability | Impact | Mitigation Strategy |
|:---|:---|:---:|:---:|:---|
| **ENG-01** | **Apple API Deprecation** <br> Apple completely deprecates the legacy `Carbon.framework` or `ApplicationServices` used for global hotkey registration in macOS 15+. | Med | High | Abstract the Hotkey Listener into its own interface (`IHotkeyListener`). If Apple deprecates Carbon, we only rewrite the internal implementation using modern `NSEvent` global monitors without touching the rest of the codebase. |
| **ENG-02** | **Compiler Dependency Hell** <br> Developers attempting to compile Tesseract/MuPDF on older macOS versions encounter C++ macro failures (e.g., the `crau_data` bug). | High | High | Strictly enforce the "Xcode 15.2+" compiler requirement established in the SRD. Long-term: bundle pre-compiled Universal binary dependencies (`.a` / `.dylib`) directly into the repository to bypass source-builds entirely. |

## 4. Accuracy & UI Constraints

| ID | Risk Description | Probability | Impact | Mitigation Strategy |
|:---|:---|:---:|:---:|:---|
| **UX-01** | **Multi-Monitor DPI Coordinate Failure** <br> The user has a 4K external monitor and a 1080p laptop screen. Qt captures the 4K screen, but draws the highlight polygon on the wrong screen due to coordinate mismatch. | Med | Med | Calculate absolute translation matrices using `QGuiApplication::screens()`. Map the `(x, y)` OCR bounding box directly to the Device Pixel Ratio (DPI) of the specific screen the active window currently resides on. |
| **UX-02** | **OCR Hallucinations & Failed Matches** <br> Tesseract struggles to read a highly stylized font, outputting "Igl" instead of "Igi", causing the search to fail. | High | Low | Implement a robust Levenshtein/Bitap fuzzy search algorithm in `src/search/` that forgives a 10-15% character deviation margin, ensuring the highlight still triggers despite minor OCR misreads. |
