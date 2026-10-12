#include <gtest/gtest.h>

#include "ocr/PdfIngestor.h"

using igi::ocr::PdfIngestor;

TEST(PdfIngestor, ReportsAvailability) {
#if defined(IGI_HAS_POPPLER)
    EXPECT_TRUE(PdfIngestor::isAvailable());
#else
    EXPECT_FALSE(PdfIngestor::isAvailable());
#endif
}

TEST(PdfIngestor, HandlesMissingFileGracefully) {
    auto results = PdfIngestor::ingestPdf(QStringLiteral("/path/to/nonexistent_file.pdf"));
    EXPECT_TRUE(results.empty());
}
