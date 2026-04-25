# Functional Requirements Document (FRD): Igi

## 1. System Initialization & Lifecycle
* **1.1 Background Daemon:** Upon execution, the Igi application must start as a headless background process (daemon) without showing a dock icon or standard application window.
* **1.2 Engine Pre-loading:** To guarantee zero-latency upon triggering, the C++ `tesseract::TessBaseAPI` must be initialized into memory with the English (`eng`) language model immediately upon daemon startup.
* **1.3 Global Hotkey Listener:** The application must register a system-wide global hotkey (e.g., `Cmd + Shift + F`) with the macOS operating system to listen for user triggers regardless of the active application.

## 2. Triggering & Visual Capture
* **2.1 Active Window Detection:** When the global hotkey is pressed, the application must hook into the macOS Accessibility API to determine the coordinates, dimensions, and document type of the currently active window.
* **2.2 RAM-Based Screen Capture (Standard):** If the window is a standard application/website, the application must use Qt's screen capture API (`QScreen::grabWindow`) to instantly copy the pixels into a volatile `QPixmap` object.
* **2.3 Direct PDF Ingestion (PDF Mode):** If the active window is detected as a PDF file (e.g., in Preview), the application must parse the absolute file path and utilize **MuPDF** to stream the document directly into memory.
* **2.4 Data Conversion Pipeline:** Both the `QPixmap` and the MuPDF data streams must be converted into a raw byte array and formatted into a Leptonica `PIX` structure, which is the required format for the Tesseract C++ API. This operation must occur strictly in memory without writing `.jpg` or `.png` files to the hard drive.

## 3. OCR Processing Pipeline
* **3.1 Visual Ingestion:** The Leptonica `PIX` structure is fed into the pre-loaded Tesseract engine using `SetImage()`.
* **3.2 Text & Coordinate Extraction:** The engine must execute `Recognize(0)` and iterate over the `ResultIterator` to extract every recognized word.
* **3.3 Bounding Box Mapping:** For every extracted word, the system must record its string value alongside its strict geometric bounding box relative to the captured image: `(x, y, width, height)`.
* **3.4 In-Memory Array:** The extracted words and bounding boxes are stored in a volatile, fast-lookup C++ array/map for the duration of the active session.

## 4. User Interface & Interaction
* **4.1 Minimalist Search Overlay:** Immediately after the OCR extraction completes (or concurrently), a frameless, borderless, always-on-top Qt `QWidget` must appear on screen containing a single, dark-mode text input field.
* **4.2 Real-Time Fuzzy Search:** As the user types into the search bar, the application must execute a fuzzy search algorithm (e.g., Levenshtein distance or Bitap) against the in-memory text array.
* **4.3 Semantic Matching:** The search algorithm must account for minor OCR errors (e.g., mistaking 'l' for '1') and return matches with a confidence score > 90%.
* **4.4 Dynamic Highlighting:** When a match is found, the application must spawn a transparent Qt overlay that precisely draws a semi-transparent yellow polygon directly over the screen coordinates `(x, y, width, height)` of the matched word.

## 5. Teardown & Security Constraints
* **5.1 Session Termination:** The active search session is terminated when the user presses `Escape`, clicks outside the search bar, or closes the active window.
* **5.2 Strict Memory Purge:** Upon termination, the application must immediately destroy the `QPixmap`, the Leptonica `PIX` structure, the search UI, the highlight overlays, and the extracted text arrays. 
* **5.3 Zero-Storage Guarantee:** The application is functionally prohibited from executing disk I/O write operations for visual or text data at any point in the workflow.
