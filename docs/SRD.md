# System Requirements Document (SRD): Igi

## 1. Operating System Specifications
* **Target OS:** macOS 13 (Ventura) or newer.
* **Architecture Support:** Universal Binary. The C++ application must compile for both Apple Silicon (ARM64) and Intel (x86_64) architectures to support a wide range of Mac hardware.
* **OS Integrations:** The application must securely integrate with the macOS `ApplicationServices.framework` and `Carbon.framework` to register low-level global system hotkeys.
* **Permissions:** The application requires explicit user authorization for **Screen Recording Permissions** (via System Settings > Privacy & Security) to execute the active-window capture API.

## 2. Hardware Constraints
* **Memory (RAM):** 
  * **Minimum:** 8GB RAM.
  * **Recommended:** 16GB+ RAM. 
  * *Justification:* As a zero-storage application, all raw uncompressed pixel data (which scales massively on 4K/5K Retina displays), Leptonica `PIX` structures, and Tesseract memory allocations must be held entirely in volatile memory. 
* **Processor (CPU):** Multi-core processor required to leverage Tesseract's multi-threaded OCR capabilities for processing large visual arrays instantaneously.

## 3. Development & Build Environment
* **Compiler:** Apple Clang version 15.0.0 or newer.
  * *Strict Requirement:* Developers must use the Command Line Tools from **Xcode 15.2+**. Older toolchains (e.g., Xcode 14) will critically fail with C++ macro compilation errors (`crau_data` bug) when attempting to build native dependencies from source on macOS 13.
* **Build Automation:** CMake 3.16 or newer.
* **Package Manager:** Homebrew (for dependency management and linking).

## 4. Core C++ Dependencies
The application relies on the following strictly version-controlled libraries:
* **Qt 6 (Version 6.5+):** Required components include `QtCore`, `QtGui`, and `QtWidgets` for managing the event loop, rendering the frameless UI overlay, and capturing the system screen.
* **Tesseract OCR (Version 5.5+):** The core engine utilized for high-speed text extraction and coordinate mapping.
* **Leptonica (Version 1.83+):** A critical dependency required by Tesseract for image pre-processing, binarization, and handling raw pixel arrays in memory.
* **MuPDF (Version 1.27+):** Maintained as the selected engine for any future implementations requiring direct file-path parsing of complex PDFs due to its lightweight C-based architecture and superior speed over Poppler.

## 5. Security & Isolation Architecture
* **Privilege Level:** The application must execute entirely in standard User Space. It does not require and must not request root/sudo elevation.
* **Disk I/O Block:** The application architecture is designed around memory-mapped buffers. It is strictly prohibited from invoking standard library file-write operations (e.g., `std::ofstream`) for any visual or text payload data.
* **Network Isolation:** The codebase must remain entirely air-gapped from network transmission libraries. No analytics, crash reporting, or cloud-sync services may be linked.
