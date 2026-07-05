#include "ocr/OcrEngine.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#ifndef Q_OS_WIN
#  include <sys/mman.h>   // mlock / munlock
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <QtConcurrent>

#if defined(Q_OS_MACOS)
#  include <CoreFoundation/CoreFoundation.h>
#endif

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "core/SecureWipe.h"
#include "search/FuzzySearch.h"

namespace igi::ocr {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string OcrEngine::resolveTessDataPath(const char* callerPath) {
    if (callerPath) return callerPath;

#if defined(Q_OS_MACOS)
    // Priority 1: app bundle Resources/tessdata.
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle) {
        CFURLRef resURL = CFBundleCopyResourcesDirectoryURL(bundle);
        if (resURL) {
            char pathBuf[PATH_MAX];
            if (CFURLGetFileSystemRepresentation(resURL, true,
                    reinterpret_cast<UInt8*>(pathBuf), sizeof(pathBuf))) {
                CFRelease(resURL);
                return std::string(pathBuf) + "/tessdata";
            }
            CFRelease(resURL);
        }
    }
#endif

    // Priority 2: TESSDATA_PREFIX env var (Homebrew default).
    if (const char* env = std::getenv("TESSDATA_PREFIX")) {
        return env;
    }

    return {};   // Empty → Tesseract uses its compiled-in default.
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

OcrEngine::OcrEngine() {
    // N=4 threads: one per TessWorker. Each thread runs exclusively on its
    // assigned worker slot (dispatch is round-robin via nextWorker_), so no
    // two threads ever share a TessBaseAPI instance.
    workerPool_.setMaxThreadCount(kOcrWorkers);
}

OcrEngine::~OcrEngine() {
    workerPool_.waitForDone();
    for (auto& w : workers_) {
        std::lock_guard<std::mutex> lk(w.mtx);
        if (w.api) w.api->End();
    }
}

bool OcrEngine::init(const char* dataPath, const char* language) {
    const std::string path = resolveTessDataPath(dataPath);
    const char* pPath = path.empty() ? nullptr : path.c_str();

    // Initialise all N workers. This is called once at startup on the main
    // thread, so no locking is needed here.
    for (auto& w : workers_) {
        w.api = std::make_unique<tesseract::TessBaseAPI>();
        if (w.api->Init(pPath, language) != 0) {
            return false;
        }
        w.api->SetPageSegMode(tesseract::PSM_AUTO);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Strip recognition (runs on a worker thread)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<search::WordBox> OcrEngine::recognizeStrip(PIX*  strip,
                                                        int   yOffset,
                                                        int   pageIndex,
                                                        int   workerIdx) {
    std::vector<search::WordBox> results;
    if (!strip) return results;

    TessWorker& w = workers_[workerIdx];
    std::lock_guard<std::mutex> lk(w.mtx);

    w.api->SetImage(strip);
    if (w.api->Recognize(nullptr) != 0) {
        w.api->Clear();
        return results;
    }

    tesseract::ResultIterator* it = w.api->GetIterator();
    if (it) {
        const auto level = tesseract::RIL_WORD;
        do {
            const char* wordText = it->GetUTF8Text(level);
            if (!wordText) continue;

            int left, top, right, bottom;
            if (it->BoundingBox(level, &left, &top, &right, &bottom)) {
                float confidence = it->Confidence(level);
                QString text = QString::fromUtf8(wordText);
                QString norm = search::FuzzySearch::normalize(text);
                results.emplace_back(
                    text, norm,
                    left, top + yOffset,       // translate back to full-image Y
                    right - left, bottom - top,
                    confidence, pageIndex
                );
            }
            delete[] wordText;
        } while (it->Next(level));
        delete it;
    }

    w.api->Clear();
    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public async entry point
// ─────────────────────────────────────────────────────────────────────────────

QFuture<std::vector<search::WordBox>>
OcrEngine::recognizeAsync(PixPtr image, int pageIndex) {
    // The outer future runs on one thread from workerPool_ and fans out to
    // N concurrent sub-futures (one per horizontal strip), then merges.
    return QtConcurrent::run(&workerPool_,
        [this, img = std::move(image), pageIndex]() mutable
            -> std::vector<search::WordBox>
        {
            if (!img) return {};

            PIX* fullPix = img.get();
            const int W  = pixGetWidth(fullPix);
            const int H  = pixGetHeight(fullPix);

            // ── Divide image into kOcrWorkers horizontal strips ───────────
            // Equal-height bands. The last strip absorbs any remainder rows.
            const int stripH = H / kOcrWorkers;

            // ── mlock the pixel buffer before dispatching ─────────────────
            // The raw PIX data must not be paged out while workers read it.
            // mlock on the Leptonica pixel words.
            l_uint32* pixData = pixGetData(fullPix);
            const std::size_t pixBytes =
                static_cast<std::size_t>(pixGetWpl(fullPix)) *
                static_cast<std::size_t>(H) * sizeof(l_uint32);
#ifndef Q_OS_WIN
            const bool mlockOk = (::mlock(pixData, pixBytes) == 0);
#else
            const bool mlockOk = (::VirtualLock(pixData, pixBytes) != 0);
#endif

            // ── Fan out: one QtConcurrent task per strip ──────────────────
            struct StripFuture {
                QFuture<std::vector<search::WordBox>> future;
            };
            std::array<StripFuture, kOcrWorkers> stripFutures;

            for (int i = 0; i < kOcrWorkers; ++i) {
                const int yStart = i * stripH;
                const int yEnd   = (i == kOcrWorkers - 1) ? H : yStart + stripH;
                const int h      = yEnd - yStart;
                const int worker = (nextWorker_.fetch_add(1,
                                        std::memory_order_relaxed))
                                   % kOcrWorkers;

                // pixCreate a sub-pix clip (shallow: shares fullPix data).
                // pixClipRectangle makes a deep copy — safe for concurrent use.
                BOX* box = boxCreate(0, yStart, W, h);
                PIX* strip = pixClipRectangle(fullPix, box, nullptr);
                boxDestroy(&box);

                const int yOffset = yStart;
                const int widx    = worker;

                stripFutures[i].future = QtConcurrent::run(&workerPool_,
                    [this, strip, yOffset, pageIndex, widx]() mutable
                        -> std::vector<search::WordBox>
                    {
                        auto result = recognizeStrip(strip, yOffset,
                                                     pageIndex, widx);
                        // Wipe and destroy the strip copy.
                        if (l_uint32* d = pixGetData(strip)) {
                            const std::size_t bytes =
                                static_cast<std::size_t>(pixGetWpl(strip)) *
                                static_cast<std::size_t>(pixGetHeight(strip)) *
                                sizeof(l_uint32);
                            igi_bzero(d, bytes);
                        }
                        pixDestroy(&strip);
                        return result;
                    });
            }

            // ── Merge results ─────────────────────────────────────────────
            std::vector<search::WordBox> merged;
            merged.reserve(static_cast<size_t>(W / 6)); // rough heuristic

            for (auto& sf : stripFutures) {
                sf.future.waitForFinished();
                auto part = sf.future.result();
                merged.insert(merged.end(),
                              std::make_move_iterator(part.begin()),
                              std::make_move_iterator(part.end()));
            }

            // Sort by reading order: top-to-bottom, then left-to-right.
            std::sort(merged.begin(), merged.end(),
                [](const search::WordBox& a, const search::WordBox& b) {
                    if (a.y != b.y) return a.y < b.y;
                    return a.x < b.x;
                });

            // ── Munlock and wipe the full PIX pixel data ──────────────────
            // img's PixPtr deleter will call igi_bzero + pixDestroy, but we
            // wipe and munlock here — before the deleter — to shorten the
            // window during which raw pixel PHI is in lockable RAM.
            igi_bzero(pixData, pixBytes);
#ifndef Q_OS_WIN
            if (mlockOk) ::munlock(pixData, pixBytes);
#else
            if (mlockOk) ::VirtualUnlock(pixData, pixBytes);
#endif
            // PixPtr destructor will call pixDestroy (data already zeroed).

            return merged;
        });
}

} // namespace igi::ocr
