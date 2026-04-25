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

### 🚧 Milestone 2: The Core Engine (Daemon & Capture)
* **Goal:** Establish the application as a background listener capable of grabbing pixels.
* **Tasks:**
  1. Configure the Qt application loop to run headlessly.
  2. Integrate macOS `Carbon` or `ApplicationServices` frameworks to register the `Cmd+Shift+F` global hotkey.
  3. Implement `QScreen::grabWindow` to capture the active window into a volatile `QPixmap` when the hotkey is fired.

### ⏳ Milestone 3: The Data Pipeline (OCR & MuPDF Integration)
* **Goal:** Pass the visual data to the OCR engine and extract coordinate arrays.
* **Tasks:**
  1. Write the memory-safe conversion function from Qt `QPixmap` to Leptonica `PIX` byte arrays.
  2. Implement MuPDF memory streaming to ingest raw bytes from active PDF file paths directly into `PIX` byte arrays.
  3. Feed the `PIX` array into the pre-loaded Tesseract engine.
  4. Map the output to a volatile C++ array containing `[WordString, X_Coord, Y_Coord, Width, Height]`.

### ⏳ Milestone 4: Search UI & Algorithmic Matching
* **Goal:** Create the user interface and connect it to the data array.
* **Tasks:**
  1. Build the frameless, always-on-top Qt search bar overlay.
  2. Implement a high-performance Levenshtein/Fuzzy distance algorithm in C++ to run upon `textChanged` events in the search bar.
  3. Return the coordinates of the highest-confidence match from the RAM array.

### ⏳ Milestone 5: Visual Highlight & Memory Teardown
* **Goal:** Physically draw the result on screen and ensure absolute data destruction.
* **Tasks:**
  1. Spawn a transparent Qt overlay over the active window.
  2. Draw a semi-transparent yellow `QPolygonF` over the exact `(x, y, width, height)` coordinates of the matched word.
  3. Implement the `Escape` key event to instantly trigger C++ destructors, destroying the UI widgets and freeing the Leptonica arrays and Qt Pixmaps from RAM.
