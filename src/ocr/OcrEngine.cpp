#include "ocr/OcrEngine.h"

#include <QtConcurrent>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

namespace igi::ocr {

OcrEngine::OcrEngine()
    : tessApi_(std::make_unique<tesseract::TessBaseAPI>()) {
    // For Chunk 3, we cap the pool to 1 thread because we only have one
    // TessBaseAPI instance. Chunk 6 will expand this to N=4 with a pool
    // of workers.
    workerPool_.setMaxThreadCount(1);
}

OcrEngine::~OcrEngine() {
    workerPool_.waitForDone();
    tessApi_->End();
}

bool OcrEngine::init(const char* dataPath, const char* language) {
    std::lock_guard<std::mutex> lock(tessMutex_);
    if (tessApi_->Init(dataPath, language) != 0) {
        return false;
    }
    // We only need bounding boxes and words, not the full layout analysis.
    // Setting PageSegMode to PSM_SPARSE_TEXT or PSM_AUTO works. AUTO is safer
    // for general screenshots.
    tessApi_->SetPageSegMode(tesseract::PSM_AUTO);
    return true;
}

QFuture<std::vector<search::WordBox>> OcrEngine::recognizeAsync(PixPtr image, int pageIndex) {
    // QtConcurrent::run posts a task to the provided QThreadPool.
    // The PixPtr is moved into the lambda to ensure it stays alive until OCR finishes,
    // and then its custom deleter runs explicit_bzero before pixDestroy.
    return QtConcurrent::run(&workerPool_, 
        [this, img = std::move(image), pageIndex]() mutable -> std::vector<search::WordBox> {
            std::vector<search::WordBox> results;
            if (!img) {
                return results;
            }

            // TessBaseAPI is not thread-safe for concurrent recognize calls.
            std::lock_guard<std::mutex> lock(tessMutex_);
            
            tessApi_->SetImage(img.get());
            
            // Recognize forces the OCR to run.
            if (tessApi_->Recognize(nullptr) != 0) {
                return results;
            }

            // Extract words via ResultIterator
            tesseract::ResultIterator* it = tessApi_->GetIterator();
            if (!it) {
                return results;
            }

            const tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
            do {
                const char* wordText = it->GetUTF8Text(level);
                if (!wordText) {
                    continue;
                }

                int left, top, right, bottom;
                if (it->BoundingBox(level, &left, &top, &right, &bottom)) {
                    float confidence = it->Confidence(level);
                    
                    results.emplace_back(
                        QString::fromUtf8(wordText),
                        left,
                        top,
                        right - left,
                        bottom - top,
                        confidence,
                        pageIndex
                    );
                }
                delete[] wordText;
            } while (it->Next(level));

            delete it;
            
            // Clear the image from Tesseract so it doesn't hold a dangling pointer
            // after the PixPtr goes out of scope and destroys the Pix.
            tessApi_->Clear();

            return results;
        });
}

} // namespace igi::ocr
