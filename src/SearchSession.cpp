#include "SearchSession.h"

#include "core/SecureWipe.h"   // igi_bzero (compiler-elision-proof wipe)
#include <QDebug>

#include "core/ScreenCapture.h"
#include "ocr/ImageConverter.h"
#include "ocr/OcrEngine.h"
#include "search/FuzzySearch.h"
#include "ui/SearchOverlay.h"

namespace igi {

SearchSession::SearchSession(
    core::IScreenCapture* capture,
    ocr::OcrEngine*       ocrEngine,
    ui::SearchOverlay*    overlay,
    QObject*              parent)
    : QObject(parent)
    , capture_(capture)
    , ocrEngine_(ocrEngine)
    , overlay_(overlay)
{
    // Wire OCR completion back to this object.
    connect(&ocrWatcher_, &QFutureWatcher<std::vector<search::WordBox>>::finished,
            this, &SearchSession::onOcrFinished);

    // Wire search-bar keystrokes to fuzzy search.
    connect(overlay_, &ui::SearchOverlay::queryChanged,
            this, &SearchSession::onQueryChanged);

    // Wire Escape to dismiss.
    connect(overlay_, &ui::SearchOverlay::dismissed,
            this, &SearchSession::dismiss);
}

void SearchSession::activate() {
    // ── 1. Capture the focused window (sync, main thread) ──
    qDebug() << "[igi] SearchSession: capturing focused window…";
    auto captureResult = capture_->captureFocusedWindow();
    if (!captureResult) {
        qWarning() << "[igi] SearchSession: capture failed — no active window.";
        return;
    }

    capturedScreenRect_ = captureResult->screenRect;
    const QImage& image = captureResult->image;

    // ── 2. Convert QImage → PIX ──
    ocr::PixPtr pix = ocr::ImageConverter::qImageToPix(image);
    if (!pix) {
        qWarning() << "[igi] SearchSession: QImage→PIX conversion failed.";
        return;
    }

    // ── 3. Launch async OCR ──
    // Drop any in-flight watcher (user pressed hotkey twice quickly).
    if (ocrWatcher_.isRunning()) {
        ocrWatcher_.cancel();
        ocrWatcher_.waitForFinished();
    }
    corpus_.clear();

    QFuture<std::vector<search::WordBox>> future =
        ocrEngine_->recognizeAsync(std::move(pix), 0);
    ocrWatcher_.setFuture(future);

    // ── 4. Show the search bar immediately so the user can start typing ──
    overlay_->activate();

    qDebug() << "[igi] SearchSession: OCR running in background…";
}

void SearchSession::dismiss() {
    // ── Secure corpus wipe (D-006 / SEC-02) ─────────────────────────────────
    //
    // PROBLEM: corpus_.clear() releases the WordBox objects, but the QString
    // heap buffers holding OCR-extracted text (potential PHI) are merely
    // returned to the allocator — the bytes remain readable in freed memory
    // until they happen to be overwritten by a future allocation.  A memory
    // forensics tool (or a process-injection attacker) running between clear()
    // and the next allocation can recover those strings verbatim.
    //
    // FIX: for every WordBox, we force QString to detach its copy-on-write
    // buffer (data() call), then run explicit_bzero() over the raw UTF-16
    // payload.  explicit_bzero is guaranteed NOT to be elided by the compiler
    // (unlike memset on a dead variable), per the POSIX rationale and Apple's
    // <string.h> documentation.  Only AFTER zeroing do we clear the vector.
    for (auto& wb : corpus_) {
        if (!wb.text.isEmpty()) {
            // data() forces detach: we now own this buffer exclusively.
            QChar* buf = wb.text.data();
            igi_bzero(buf, static_cast<size_t>(wb.text.size()) * sizeof(QChar));
        }
        if (!wb.normalizedText.isEmpty()) {
            QChar* buf = wb.normalizedText.data();
            igi_bzero(buf, static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar));
        }
    }

    munlockAndWipeCorpus();
    capturedScreenRect_ = {};

    qDebug() << "[igi] SearchSession: dismissed — corpus securely wiped.";
}

// ── mlock / munlock helpers ───────────────────────────────────────────────────
//
// SEC-01 (RISK_REGISTER.md): mlock() pins pages in physical RAM so the OS
// cannot write them to the swap file under memory pressure.  Without this,
// even explicit_bzero() cannot guarantee the data wasn't already written to
// the SSD in a previous page-out cycle.
//
// We mlock each QString's individual heap buffer because std::vector<WordBox>
// stores WordBox objects contiguously, but each WordBox contains two QStrings
// whose UTF-16 data lives in separate heap allocations — not in the vector's
// own buffer. We must lock each one individually.
//
// mlock() is limited by the process's RLIMIT_MEMLOCK ulimit. On macOS the
// default is ~8 MB per process, which is well above any OCR corpus for a
// single screen. If mlock() fails we log a warning and continue — the
// explicit_bzero wipe on dismiss() still runs as the secondary control.
void SearchSession::mlockCorpus() {
    int failCount = 0;
    for (const auto& wb : corpus_) {
        if (!wb.text.isEmpty()) {
            const void* p = wb.text.constData();
            if (::mlock(p, static_cast<size_t>(wb.text.size()) * sizeof(QChar)) != 0) {
                ++failCount;
            }
        }
        if (!wb.normalizedText.isEmpty()) {
            const void* p = wb.normalizedText.constData();
            if (::mlock(p, static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar)) != 0) {
                ++failCount;
            }
        }
    }
    corpusLocked_ = true;
    if (failCount > 0) {
        qWarning() << "[igi] mlock() failed for" << failCount
                   << "buffers. Corpus may be swappable. "
                      "Check RLIMIT_MEMLOCK (ulimit -l).";
    } else {
        qDebug() << "[igi] Corpus mlock()'d —" << corpus_.size()
                 << "WordBoxes pinned in RAM.";
    }
}

void SearchSession::munlockAndWipeCorpus() {
    // ── Step 1: explicit_bzero every QString buffer ──
    // data() forces QString copy-on-write detach so we own the buffer.
    // explicit_bzero cannot be compiler-elided (unlike memset on dead stores).
    for (auto& wb : corpus_) {
        if (!wb.text.isEmpty()) {
            QChar* buf = wb.text.data();
            igi_bzero(buf, static_cast<size_t>(wb.text.size()) * sizeof(QChar));
        }
        if (!wb.normalizedText.isEmpty()) {
            QChar* buf = wb.normalizedText.data();
            igi_bzero(buf, static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar));
        }
    }

    // ── Step 2: munlock so the OS can reclaim the pages ──
    // We munlock AFTER zeroing, not before — if we unlocked first, the OS
    // could theoretically page-out the zeroed page before we zero it
    // (extremely unlikely but architecturally possible under heavy pressure).
    if (corpusLocked_) {
        for (const auto& wb : corpus_) {
            if (!wb.text.isEmpty()) {
                ::munlock(wb.text.constData(),
                          static_cast<size_t>(wb.text.size()) * sizeof(QChar));
            }
            if (!wb.normalizedText.isEmpty()) {
                ::munlock(wb.normalizedText.constData(),
                          static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar));
            }
        }
        corpusLocked_ = false;
    }

    // ── Step 3: release the vector ──
    corpus_.clear();
    corpus_.shrink_to_fit();
}



void SearchSession::onOcrFinished() {
    corpus_ = ocrWatcher_.result();
    qDebug() << "[igi] SearchSession: OCR complete," << corpus_.size() << "words extracted.";

    // Pin corpus pages in RAM immediately — before any search occurs.
    // Prevents the OS from writing PHI to the swap file (SEC-01).
    mlockCorpus();

    // Re-run search with whatever the user has typed while OCR was running.
    onQueryChanged(overlay_->query());
}


void SearchSession::onQueryChanged(const QString& query) {
    if (corpus_.empty() || query.trimmed().isEmpty()) {
        // No corpus yet or empty query: clear highlights.
        overlay_->setHighlights({}, capturedScreenRect_);
        return;
    }

    // ── Fuzzy search (always on main thread, stays within 16 ms budget) ──
    const auto matches = search::FuzzySearch::topK(query, corpus_, 20, 0.70f);

    // Convert WordBox pixel rects to absolute screen rects by offsetting
    // by the captured window's top-left corner.
    const QPoint origin = capturedScreenRect_.topLeft();
    std::vector<QRect> highlightRects;
    highlightRects.reserve(matches.size());

    for (const auto& m : matches) {
        const auto& wb = m.word;
        highlightRects.emplace_back(
            origin.x() + wb.x,
            origin.y() + wb.y,
            wb.w,
            wb.h);
    }

    overlay_->setHighlights(highlightRects, capturedScreenRect_);

    qDebug() << "[igi] SearchSession:" << matches.size()
             << "match(es) for" << query;
}

}  // namespace igi
