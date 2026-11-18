// ==============================================================================
// Program: BoundingBox.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Defines spatial bounding box geometry structures, screen coordinate transformations, normalized word box representations, and confidence metrics for OCR search hits.
// ==============================================================================

#pragma once

#include <QString>

namespace igi::search {

struct WordBox {
    QString text;
    QString normalizedText;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float confidence = 0.0f; // 0.0 to 100.0
    int pageIndex = 0;

    WordBox() = default;

    WordBox(QString t, QString norm, int x, int y, int w, int h, float conf, int pageIdx = 0)
        : text(std::move(t)), normalizedText(std::move(norm)), x(x), y(y), w(w), h(h), confidence(conf), pageIndex(pageIdx) {}
};

} // namespace igi::search
