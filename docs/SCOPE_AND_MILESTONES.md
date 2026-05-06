# Igi: Scope Lock & Milestones

To prevent scope creep and ensure adherence to the ultra-low latency, zero-storage architecture, this document strictly defines what is permitted within the project boundaries and establishes rigid engineering milestones for development.

---

## 1. Scope Lock (The Boundaries)

### 🟢 In-Scope (Strictly Required)
1. **Target Platform:** Native macOS application (Universal Binary).
2. **Core Operation:** Background daemon listening for a global macOS hotkey.
3. **Data Source:** In-memory pixel capture of the currently active window/screen region via Qt.
4. **Data Processing:** Native C++ integration with Tesseract and Leptonica for localized, multi-threaded OCR.
5. **Search Logic:** Real-time fuzzy search (Levenshtein distance / Bitap) with >90% confidence matching.
6. **UI/UX:** Frameless, dark-mode Qt search overlay and transparent geometric highlight polygons.
7. **Security:** Immediate volatile RAM purging upon search termination.

### 🔴 Out-of-Scope (Strictly Prohibited)
1. **Platforms:** No Windows or Linux compatibility.
2. **Cloud APIs:** No reliance on AWS Textract, Google Cloud Vision, or any external internet-based OCR APIs.
3. **Storage:** No saving screenshots, log caches, or extracted text arrays to the local hard drive.
4. **Tracking:** No telemetry, analytics, or user-tracking libraries.
5. **PDF File Editors:** No tools for annotating, saving, or exporting PDFs.

---

## 2. Engineering Milestones

### ✅ Milestone 1: Environment & Architecture Scaffolding (COMPLETED)
* Define Problem Statement, PRD, FRD, and SRD.
* Install and link highly specific macOS dependencies (Qt, Tesseract, Leptonica, MuPDF) using Xcode 15.2 tools.
* Scaffold initial `CMakeLists.txt` and verify C++ linkage with a successful `main.cpp` build.

### ✅ Milestone 2: The Core Engine (Daemon & Capture) (COMPLETED)
* **Goal:** Establish the application as a background listener capable of grabbing pixels.
* **Tasks:**
  1. Configure the Qt application loop to run headlessly. ✅
  2. Register the `Cmd+Shift+F` global hotkey via `NSEvent.addGlobalMonitorForEventsMatchingMask` (primary). Carbon's `RegisterEventHotKey` is retained as a compiled fallback only (see `docs/DECISIONS.md` D-004). ✅
  3. Implemented `CGWindowListCreateImage` to capture the active window into a volatile `QImage` when the hotkey is fired. ✅

### ✅ Milestone 3: The Data Pipeline (OCR Integration) (COMPLETED)
* **Goal:** Pass the visual data to the OCR engine and extract coordinate arrays.
* **Tasks:**
  1. Wrote `ImageConverter::qImageToPix()` — memory-safe `QImage` → Leptonica `PIX` with `explicit_bzero` teardown (D-006). ✅
  2. Fed `PIX` into pre-loaded Tesseract engine via `OcrEngine::recognizeAsync()` on a `QThreadPool`. ✅
  3. Mapped output to a volatile `std::vector<WordBox>` containing `[text, x, y, w, h, confidence, pageIndex]`. ✅

### ✅ Milestone 4: Search UI & Algorithmic Matching (COMPLETED)
* **Goal:** Create the user interface and connect it to the data array.
* **Tasks:**
  1. Built `SearchOverlay` — frameless, always-on-top dark glassmorphism Qt search pill. ✅
  2. Implemented Wagner-Fischer fuzzy search (D-001) running on `textChanged` events, within 16 ms budget (D-002). ✅
  3. `FuzzySearch::topK()` returns the top-K coordinate matches from the in-RAM corpus. ✅

### ✅ Milestone 5: Visual Highlight & Memory Teardown (COMPLETED)
* **Goal:** Physically draw the result on screen and ensure absolute data destruction.
* **Tasks:**
  1. Spawned `HighlightOverlay` — a transparent, click-through `QWidget` positioned over the captured window. ✅
  2. Draws amber rounded rectangles over the exact `(x, y, w, h)` coordinates of each fuzzy match. ✅
  3. `Escape` key triggers `corpus_.clear()` + `shrink_to_fit()`, destroying all OCR data from RAM (D-006, D-007). ✅

### ✅ Milestone 6: Distribution & Security (COMPLETED)
* **Goal:** Produce a Gatekeeper-clean, distributable macOS app with zero-write enforcement.
* **Tasks:**
  1. `Info.plist` with `LSUIElement=1`, privacy usage strings, min macOS 13. ✅
  2. `Igi.entitlements` — screen-capture + accessibility only; no network or file-write entitlements. ✅
  3. `eng.traineddata` bundled into `Resources/tessdata`; `OcrEngine` reads from bundle at runtime. ✅
  4. `macdeployqt` run post-build to embed all Qt frameworks. ✅
  5. `bundle.sh` — convenience build + DMG packaging script. ✅
  6. `scripts/sign_and_notarize.sh` — ad-hoc (local) and Developer ID + notarization (CI) signing. ✅
  7. `scripts/sandbox_test.sh` + `resources/igi.sb` — D-007 write-deny test under `sandbox-exec`. ✅
  8. Two-job CI: `build-and-test` + `bundle-sign-sandbox` with conditional notarization on `main`. ✅
