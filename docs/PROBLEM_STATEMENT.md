# Igi: Problem Statement & Scope Definition

## The Problem
Standard text-search functionalities (such as `Ctrl+F`) are fundamentally limited because they rely on underlying encoded text data rather than visual information. This causes severe friction in modern workflows, specifically when users encounter:
1. **Scanned Documents & Faxes:** Non-searchable medical records, flattened PDFs, or historical archives.
2. **Stylized Interfaces:** Complex web dashboards or applications where text is rendered as graphic elements.
3. **Remote Environments:** Virtual desktop infrastructures (e.g., Citrix, RDP) where data is streamed exclusively as visual pixels rather than readable text.

When attempting to locate specific information in these unsearchable environments, users are forced to manually scan pages of visual data. Attempting to use traditional cloud-based OCR tools or file-drop utilities introduces severe security risks, as saving temporary files or transmitting data to external servers explicitly violates strict corporate security policies and HIPAA regulations.

## The Solution
We must engineer a **Context-Aware Visual Search Utility** that operates directly on the machine's visual layer, completely bypassing the file system. 

When triggered—either via a global system hotkey or by launching the minimalist application—the tool must execute the following workflow:

1. **Contextual Ingestion:** Seamlessly capture the visual state of the currently active window directly into volatile RAM via Qt screen capture. If the active window is detected as a native PDF viewer, instantly intercept the file path and stream the document directly into memory via MuPDF.
2. **High-Performance Processing:** Utilize a highly optimized, multi-threaded C++ OCR engine to map visual text to screen coordinates with near-zero latency.
3. **Minimalist Overlay:** Present an ultra-minimalist, floating search bar that fuzzy-matches the user's query against the OCR'd text and physically draws a visual highlight over the target word on their screen.
4. **Zero-Storage Architecture:** Purge all visual data and text arrays from memory the exact millisecond the search is closed or completed. 

By strictly prohibiting temporary files, caches, and disk writes (note: reading user-opened files and the bundled Tesseract model is allowed — see `docs/DECISIONS.md` D-007), this architecture delivers a fully offline, HIPAA-aligned search experience suitable for organizations whose policies forbid third-party storage of PHI. Final HIPAA compliance remains a property of the deploying organization's overall safeguards, not Igi alone.
