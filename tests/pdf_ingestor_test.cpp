// Program: igi - pdf_ingestor_test.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Tests PdfIngestor availability reporting and handling of a missing file.

#include <gtest/gtest.h>

#include "ocr/PdfIngestor.h"

using igi::ocr::PdfIngestor;

// isAvailable reports true only when the build includes Poppler support.
TEST(PdfIngestor, ReportsAvailability) {
#if defined(IGI_HAS_POPPLER)
    EXPECT_TRUE(PdfIngestor::isAvailable());
#else
    EXPECT_FALSE(PdfIngestor::isAvailable());
#endif
}

// A missing file yields an empty result.
TEST(PdfIngestor, HandlesMissingFileGracefully) {
    auto results = PdfIngestor::ingestPdf(QStringLiteral("/path/to/nonexistent_file.pdf"));
    EXPECT_TRUE(results.empty());
}
