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
