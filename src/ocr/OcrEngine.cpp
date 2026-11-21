// ==============================================================================
// Program: OcrEngine.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Manages the Tesseract OCR engine lifecycle, asynchronous page segmentation, worker thread pooling, character confidence filtering, and word bounding box extraction.
//
// How it fits: main.cpp creates and init()s one OcrEngine; SearchSession calls
// recognizeAsync() with a Leptonica PIX made by ImageConverter and gets a
// QFuture of WordBox (text + pixel rectangle + confidence). The class
// declaration and the thread-safety rationale are in OcrEngine.h.
//
// Algorithm: the image is cut into kOcrWorkers (4) horizontal strips, each
// strip is recognised by its own TessBaseAPI instance on its own thread
// (TessBaseAPI instances are not thread-safe), then the per-strip results are
// merged and sorted into reading order (top to bottom, left to right).
//
// Security: pixel buffers are pinned with mlock() while in use and zeroed with
// igi_bzero() afterwards, because screenshots may show sensitive data.
//
// Known limits: words that straddle a strip boundary are cut in two and may be
// recognised as fragments or missed; the outer task and its strip tasks share
// the same 4-thread pool, so total OCR concurrency is bounded by it.
// ==============================================================================

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

// QtConcurrent provides run() for the thread pool and QFuture results.
#include <QtConcurrent>
#include <QDir>

#if defined(Q_OS_MACOS)
#  include <CoreFoundation/CoreFoundation.h>
#endif

// Tesseract (OCR) and Leptonica (image) C++ APIs from the installed libraries.
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "core/SecureWipe.h"
#include "search/FuzzySearch.h"

namespace igi::ocr {

// Helpers
// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Decides where Tesseract should look for its language data (tessdata).
 * Order: an explicit caller path; on macOS the app bundle's Resources/tessdata
 * (so the packaged app is self-contained); the TESSDATA_PREFIX environment
 * variable (Homebrew default); otherwise empty, meaning Tesseract's built-in default.
 * @param callerPath Path given by the caller, or nullptr.
 * @return Directory path, or an empty string for "use the default".
 */
std::string OcrEngine::resolveTessDataPath(const char* callerPath) {
    if (callerPath) return callerPath;

    // Priority 1: explicit TESSDATA_PREFIX env var if directory exists.
    if (const char* env = std::getenv("TESSDATA_PREFIX")) {
        if (QDir(QString::fromLocal8Bit(env)).exists()) {
            return env;
        }
    }

#if defined(Q_OS_MACOS)
    // Priority 2: app bundle Resources/tessdata (packaged self-contained app).
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle) {
        CFURLRef resURL = CFBundleCopyResourcesDirectoryURL(bundle);
        if (resURL) {
            char pathBuf[PATH_MAX];
            if (CFURLGetFileSystemRepresentation(resURL, true,
                    reinterpret_cast<UInt8*>(pathBuf), sizeof(pathBuf))) {
                CFRelease(resURL);
                std::string candidate = std::string(pathBuf) + "/tessdata";
                if (QDir(QString::fromStdString(candidate)).exists()) {
                    return candidate;
                }
            } else {
                CFRelease(resURL);
            }
        }
    }

    // Priority 3: standard Homebrew tessdata paths on macOS.
    for (const char* p : {"/opt/homebrew/share/tessdata", "/usr/local/share/tessdata"}) {
        if (QDir(QString::fromLocal8Bit(p)).exists()) {
            return p;
        }
    }
#endif

#if defined(Q_OS_LINUX)
    // Priority 3: standard distribution tessdata paths on Linux.
    for (const char* p : {
        "/usr/share/tesseract-ocr/5/tessdata",
        "/usr/share/tesseract-ocr/4.00/tessdata",
        "/usr/share/tessdata",
        "/usr/local/share/tessdata"
    }) {
        if (QDir(QString::fromLocal8Bit(p)).exists()) {
            return p;
        }
    }
#endif

    return {};   // Empty → Tesseract uses its compiled-in default.
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Prepares the worker pool: at most kOcrWorkers threads, one per Tesseract
 * instance. Dispatch is round-robin (nextWorker_), so no two threads use the
 * same TessBaseAPI at the same time. Tesseract itself is loaded in init().
 */
OcrEngine::OcrEngine() {
    // N=4 threads: one per TessWorker. Each thread runs exclusively on its
    // assigned worker slot (dispatch is round-robin via nextWorker_), so no
    // two threads ever share a TessBaseAPI instance.
    workerPool_.setMaxThreadCount(kOcrWorkers);
}

/** Waits for running OCR tasks, then releases every Tesseract instance. */
OcrEngine::~OcrEngine() {
    workerPool_.waitForDone();
    for (auto& w : workers_) {
        std::lock_guard<std::mutex> lk(w.mtx);
        if (w.api) w.api->End();
    }
}

/**
 * Loads the language model into every worker's TessBaseAPI.
 * @param dataPath  tessdata directory, or nullptr to auto-resolve.
 * @param language  Tesseract language code, e.g. "eng".
 * @return true if all workers initialised; false if any Init failed (the engine
 *         then stays uninitialised and recognition returns nothing).
 */
bool OcrEngine::init(const char* dataPath, const char* language) {
    initialized_.store(false);
    const std::string path = resolveTessDataPath(dataPath);
    const char* pPath = path.empty() ? nullptr : path.c_str();

    // Called once at start-up on the main thread, before any worker runs, so
    // no locking is needed here.
    for (auto& w : workers_) {
        w.api = std::make_unique<tesseract::TessBaseAPI>();
        if (w.api->Init(pPath, language) != 0) {
            return false;
        }
        // PSM_SPARSE_TEXT: find as much text as possible in no particular order. Screens
        // are not page-like, so a sparse mode finds labels and UI text that a
        // paragraph mode would skip.
        w.api->SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
    }
    initialized_.store(true);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Strip recognition (runs on a worker thread)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Recognises one horizontal strip with the worker assigned to it.
 * @param strip     Leptonica image of the strip (not owned; caller destroys).
 * @param yOffset   Y of the strip inside the full image; added to every box so
 *                  results are in full-image coordinates.
 * @param pageIndex Value stored in every WordBox (0 for screenshots).
 * @param workerIdx Which Tesseract instance to use (0..kOcrWorkers-1).
 * @return Words with confidence and pixel rectangles; empty on any failure.
 * Takes the worker's mutex, so concurrent callers for the same worker queue up.
 */
std::vector<search::WordBox> OcrEngine::recognizeStrip(PIX*  strip,
                                                        int   yOffset,
                                                        int   pageIndex,
                                                        int   workerIdx) {
    std::vector<search::WordBox> results;
    if (!strip) return results;

    TessWorker& w = workers_[workerIdx];
    std::lock_guard<std::mutex> lk(w.mtx);
    // If init() failed or never ran there is nothing to recognise with.
    if (!initialized_.load() || !w.api) return results;

    // Recognize() == 0 means success; on failure reset the instance for the next use.
    w.api->SetImage(strip);
    if (w.api->Recognize(nullptr) != 0) {
        w.api->Clear();
        return results;
    }

    tesseract::ResultIterator* it = w.api->GetIterator();
    if (it) {
        // Word level iteration: one WordBox per recognised word.
        const auto level = tesseract::RIL_WORD;
        do {
            const char* wordText = it->GetUTF8Text(level);
            // A null text means no word here; skip (the do/while condition still advances).
            if (!wordText) continue;

            int left, top, right, bottom;
            if (it->BoundingBox(level, &left, &top, &right, &bottom)) {
                float confidence = it->Confidence(level);
                QString text = QString::fromUtf8(wordText);
                QString norm = search::FuzzySearch::normalize(text);
                results.emplace_back(
                    text, norm,
                    // Tesseract's Y is relative to the strip; add the strip's offset in the full image.
                    left, top + yOffset,       // translate back to full-image Y
                    right - left, bottom - top,
                    confidence, pageIndex
                );
            }
            // GetUTF8Text returns a new[] buffer that the caller must free.
            delete[] wordText;
        } while (it->Next(level));
        delete it;
    }

    // Release the image reference and recognition results so the next strip starts clean.
    w.api->Clear();
    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public async entry point
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Starts OCR of a whole image in the background.
 * @param image     Image to read; ownership moves into the task, which wipes it afterwards.
 * @param pageIndex Page number stored in each WordBox (0 for screenshots).
 * @return Future that resolves to all words sorted top-to-bottom, left-to-right
 *         (empty if the image is empty or the engine is not initialised).
 */
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

            if (W <= 0 || H <= 0) return {};

            // Divide the image into kOcrWorkers equal-height bands; the last band absorbs
            // any remainder rows (so strips can differ by up to kOcrWorkers-1 rows).
            const int stripH = std::max(1, H / kOcrWorkers);

            // Pin the pixel buffer (mlock / VirtualLock) so it cannot be written to swap
            // while workers read it. Failure is tolerated (mlockOk records the outcome).
            l_uint32* pixData = pixGetData(fullPix);
            const std::size_t pixBytes =
                static_cast<std::size_t>(pixGetWpl(fullPix)) *
                static_cast<std::size_t>(H) * sizeof(l_uint32);
#ifndef Q_OS_WIN
            const bool mlockOk = (::mlock(pixData, pixBytes) == 0);
#else
            const bool mlockOk = (::VirtualLock(pixData, pixBytes) != 0);
#endif

            // Fan out: one QtConcurrent task per strip.
            struct StripFuture {
                QFuture<std::vector<search::WordBox>> future;
                bool valid = false;
            };
            // One slot per strip; `valid` marks the ones actually started.
            std::array<StripFuture, kOcrWorkers> stripFutures;

            for (int i = 0; i < kOcrWorkers; ++i) {
                const int yStart = i * stripH;
                if (yStart >= H) break;
                const int yEnd   = (i == kOcrWorkers - 1) ? H : std::min(H, yStart + stripH);
                const int h      = yEnd - yStart;
                if (h <= 0) break;

                // Round-robin worker choice. relaxed ordering is enough: it only spreads load, the
                // per-worker mutex provides the real exclusion.
                const int worker = (nextWorker_.fetch_add(1,
                                        std::memory_order_relaxed))
                                   % kOcrWorkers;

                // pixClipRectangle makes a deep copy of the strip's pixels, so each worker
                // owns private data and strips can be processed concurrently.
                BOX* box = boxCreate(0, yStart, W, h);
                if (!box) continue;
                PIX* strip = pixClipRectangle(fullPix, box, nullptr);
                boxDestroy(&box);
                if (!strip) continue;

                const int yOffset = yStart;
                const int widx    = worker;

                stripFutures[i].valid = true;
                stripFutures[i].future = QtConcurrent::run(&workerPool_,
                    [this, strip, yOffset, pageIndex, widx]() mutable
                        -> std::vector<search::WordBox>
                    {
                        auto result = recognizeStrip(strip, yOffset,
                                                     pageIndex, widx);
                        // Wipe and destroy the strip copy (it holds a piece of the screenshot).
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

            // Merge results: wait for every strip in order and concatenate.
            std::vector<search::WordBox> merged;
            // Rough size hint only (about one word per six pixels of width).
            merged.reserve(static_cast<size_t>(W / 6)); // rough heuristic

            for (auto& sf : stripFutures) {
                if (!sf.valid) continue;
                sf.future.waitForFinished();
                auto part = sf.future.result();
                merged.insert(merged.end(),
                              std::make_move_iterator(part.begin()),
                              std::make_move_iterator(part.end()));
            }

            // Sort by reading order: top-to-bottom, then left-to-right. (Words from the
            // same line can differ by a pixel or two in y, so this is approximate.)
            std::sort(merged.begin(), merged.end(),
                [](const search::WordBox& a, const search::WordBox& b) {
                    if (a.y != b.y) return a.y < b.y;
                    return a.x < b.x;
                });

            // Wipe and unlock the full image's pixels now (before the PixPtr deleter runs
            // pixDestroy) to shorten the time raw screenshot data stays resident.
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
