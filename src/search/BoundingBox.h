// ==============================================================================
// Program: BoundingBox.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Defines spatial bounding box geometry structures, screen coordinate transformations, normalized word box representations, and confidence metrics for OCR search hits.
// ==============================================================================

//
// How it fits: WordBox is the common currency of the pipeline. OcrEngine and
// PdfIngestor produce vectors of them, SearchSession stores them as its corpus,
// FuzzySearch ranks them and SearchSession turns their rectangles into overlay
// highlights. Header only.
#pragma once

#include <QString>

namespace igi::search {

/**
 * One recognised word with where it is on the captured page.
 * Coordinates are PHYSICAL pixels relative to the top-left of the captured
 * image (SearchSession converts to logical screen coordinates).
 */
struct WordBox {
    // Text as recognised (shown to the user).
    QString text;
    // Lower-cased, punctuation-free form used for matching (FuzzySearch::normalize).
    QString normalizedText;
    // Rectangle: left, top, width, height in pixels.
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    // Recogniser confidence as reported by Tesseract, 0..100 (not 0..1).
    float confidence = 0.0f; // 0.0 to 100.0
    // Source page: 0 for a screenshot, the 1-BASED page number for words taken from
    // a PDF (PdfIngestor). SearchSession shows it in the match counter when > 0.
    int pageIndex = 0;

    WordBox() = default;

    /** Convenience constructor taking every field; strings are moved. */
    WordBox(QString t, QString norm, int x, int y, int w, int h, float conf, int pageIdx = 0)
        : text(std::move(t)), normalizedText(std::move(norm)), x(x), y(y), w(w), h(h), confidence(conf), pageIndex(pageIdx) {}
};

} // namespace igi::search
