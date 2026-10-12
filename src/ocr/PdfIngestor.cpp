#include "ocr/PdfIngestor.h"

#include <QDebug>
#include <QFile>
#include <QImage>

#if defined(IGI_HAS_POPPLER)
#  include <poppler-qt6.h>
#endif

#include "ocr/ImageConverter.h"
#include "ocr/OcrEngine.h"
#include "search/FuzzySearch.h"

namespace igi::ocr {

bool PdfIngestor::isAvailable() {
#if defined(IGI_HAS_POPPLER)
    return true;
#else
    return false;
#endif
}

std::vector<search::WordBox> PdfIngestor::ingestPdf(const QString& pdfPath, OcrEngine* ocrEngine) {
    std::vector<search::WordBox> results;

    if (pdfPath.isEmpty() || !QFile::exists(pdfPath)) {
        qWarning() << "[igi] PdfIngestor: invalid or missing PDF path:" << pdfPath;
        return results;
    }

#if defined(IGI_HAS_POPPLER)
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(pdfPath);
    if (!doc || doc->isLocked()) {
        qWarning() << "[igi] PdfIngestor: failed to load or locked PDF:" << pdfPath;
        return results;
    }

    const int numPages = doc->numPages();
    qDebug() << "[igi] PdfIngestor: ingesting PDF" << pdfPath << "with" << numPages << "pages…";

    for (int i = 0; i < numPages; ++i) {
        std::unique_ptr<Poppler::Page> page(doc->page(i));
        if (!page) continue;

        const int pageNumber = i + 1; // 1-based page index
        auto textList = page->textList();

        if (!textList.empty()) {
            // Page has digital vector text!
            for (const auto& tb : textList) {
                if (!tb) continue;
                QString text = tb->text();
                if (text.trimmed().isEmpty()) continue;

                QString norm = search::FuzzySearch::normalize(text);
                QRectF r = tb->boundingBox();

                results.emplace_back(
                    text, norm,
                    static_cast<int>(r.x()),
                    static_cast<int>(r.y()),
                    static_cast<int>(r.width()),
                    static_cast<int>(r.height()),
                    1.0f, // 100% confidence for digital text
                    pageNumber
                );
            }
        } else if (ocrEngine) {
            // Scanned image page: render to QImage at 150 DPI and OCR
            QImage img = page->renderToImage(150.0, 150.0);
            if (!img.isNull()) {
                if (img.format() != QImage::Format_ARGB32) {
                    img = img.convertToFormat(QImage::Format_ARGB32);
                }
                PixPtr pix = ImageConverter::qImageToPix(img);
                if (pix) {
                    QFuture<std::vector<search::WordBox>> future = ocrEngine->recognizeAsync(std::move(pix), pageNumber);
                    auto pageWords = future.result();
                    results.insert(results.end(),
                                   std::make_move_iterator(pageWords.begin()),
                                   std::make_move_iterator(pageWords.end()));
                }
            }
        }
    }

    qDebug() << "[igi] PdfIngestor: complete." << results.size()
             << "total word boxes extracted from" << numPages << "pages.";
#else
    qWarning() << "[igi] PdfIngestor: compiled without Poppler support.";
#endif

    return results;
}

} // namespace igi::ocr
