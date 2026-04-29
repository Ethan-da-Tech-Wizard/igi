#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include <QFuture>
#include <QThreadPool>

#include "ocr/ImageConverter.h"
#include "search/BoundingBox.h"

namespace tesseract {
class TessBaseAPI;
}

namespace igi::ocr {

class OcrEngine {
public:
    OcrEngine();
    ~OcrEngine();

    // Disable copy/move semantics since we own a heavy C++ resource.
    OcrEngine(const OcrEngine&) = delete;
    OcrEngine& operator=(const OcrEngine&) = delete;
    OcrEngine(OcrEngine&&) = delete;
    OcrEngine& operator=(OcrEngine&&) = delete;

    // Initializes Tesseract with the given language model.
    // Returns true on success.
    bool init(const char* dataPath = nullptr, const char* language = "eng");

    // Processes the image asynchronously. 
    // The PixPtr takes ownership of the Leptonica image and securely destroys it
    // on completion.
    QFuture<std::vector<search::WordBox>> recognizeAsync(PixPtr image, int pageIndex = 0);

private:
    std::unique_ptr<tesseract::TessBaseAPI> tessApi_;
    QThreadPool workerPool_;
    std::mutex tessMutex_; // Protects access to the shared TessBaseAPI instance.
};

} // namespace igi::ocr
