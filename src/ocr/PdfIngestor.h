// Program: igi - PdfIngestor.h
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares PdfIngestor, which extracts word boxes from PDFs via Poppler with an OCR fallback.

#pragma once

#include <memory>
#include <vector>
#include <QString>

#include "search/BoundingBox.h"

namespace igi::ocr {

class OcrEngine;

class PdfIngestor {
public:
    // Returns true if Poppler support is compiled in and functional.
    static bool isAvailable();

    // Ingests a PDF file from disk, extracting digital text via Poppler
    // and running OCR fallback on scanned/image-only pages.
    // Returns a vector of WordBox entries tagged with 1-based pageIndex.
    static std::vector<search::WordBox> ingestPdf(const QString& pdfPath, OcrEngine* ocrEngine = nullptr);
};

} // namespace igi::ocr
