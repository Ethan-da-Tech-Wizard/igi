#include "SearchSession.h"

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
            explicit_bzero(buf, static_cast<size_t>(wb.text.size()) * sizeof(QChar));
        }
        if (!wb.normalizedText.isEmpty()) {
            QChar* buf = wb.normalizedText.data();
            explicit_bzero(buf, static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar));
        }
    }

    corpus_.clear();
    corpus_.shrink_to_fit();  // Release the vector's heap allocation entirely.
    capturedScreenRect_ = {};

    qDebug() << "[igi] SearchSession: dismissed — corpus securely wiped.";
}


void SearchSession::onOcrFinished() {
    corpus_ = ocrWatcher_.result();
    qDebug() << "[igi] SearchSession: OCR complete," << corpus_.size() << "words extracted.";

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
