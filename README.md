# ⚡ Igi OCR (High-Performance Visual Search)

**The Problem:** Scanned documents, complex images, and PDFs with stylized characters cannot be semantically searched using the standard `Ctrl+F` function. 

**The Solution:** Igi is an ultra-low latency, C++ powered utility that provides visual `Ctrl+F` functionality on **any** document, regardless of how it was scanned or formatted. 

## 🎯 Core Capabilities

To be perfectly clear on what this tool is and what it does:

1. **Ultra-Low Latency Searches:** Built from the ground up in C++ to provide instantaneous semantic searches. It parses heavily scanned documents in milliseconds.
2. **Context-Aware Global Utility:** There is no "file uploader" or clunky UI. Igi runs silently in the background and instantly reads the currently active website or open PDF on your screen when summoned.
3. **Universal Highlighting:** It has the precise capacity to read the document and visually "highlight" the exact requested word directly over your active window.
4. **100% Offline Capable:** It does not require an internet connection. The C++ engine is completely self-contained and air-gapped.
5. **Looks at Data, Never Stores Data:** It looks at external data to perform the search, but it **never, ever stores external data**. There are no temp files, no caches, and no disk writes. It operates entirely in volatile RAM.

## 🚀 How It Works

Igi operates as a pure utility. It does not try to be a clunky document manager. 

1. You have an unsearchable 70-page scanned PDF open, or a heavily stylized website.
2. You activate Igi via a global hotkey (e.g., `Cmd+Shift+F`).
3. Igi instantly detects the active window. For websites, it pulls visual data directly from the screen. For PDFs, it seamlessly detects the open file and ingests it into volatile RAM.
4. You type your search term in the minimalist floating search bar.
5. Igi instantly finds the semantic match and draws a highlight over the word directly on your screen. 
6. When you close the search, the memory is instantly purged. Nothing is left behind.

## 🛠 Technology Stack

This tool requires raw power and memory control. It is built using:
- **C++ (Qt Framework):** For the lightning-fast, cross-platform user interface, global hotkey detection, and floating search bar overlay.
- **C++ (libtesseract):** The raw, native OCR engine for high-performance text extraction.
- **C++ (MuPDF/Leptonica):** For securely ingesting complex PDFs and active-window captures directly into RAM without touching the hard drive. MuPDF is used for its superior speed and lightweight memory footprint.
