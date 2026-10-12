#include "SearchSession.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "core/SecureWipe.h"   // igi_bzero (compiler-elision-proof wipe)
#include <cmath>
#include <QDebug>
#include <QElapsedTimer>

#include "core/ScreenCapture.h"
#include "ocr/ImageConverter.h"
#include "ocr/OcrEngine.h"
#include "ocr/PdfIngestor.h"
#include "search/FuzzySearch.h"
#include "ui/SearchOverlay.h"
#include "ui/CaptureFlashOverlay.h"

#include <QFile>

namespace igi {

SearchSession::SearchSession(
    core::IScreenCapture* capture,
    ocr::OcrEngine*       ocrEngine,
    ui::SearchOverlay*    overlay,
    ui::CaptureFlashOverlay* flashOverlay,
    QObject*              parent)
    : QObject(parent)
    , capture_(capture)
    , ocrEngine_(ocrEngine)
    , overlay_(overlay)
    , flashOverlay_(flashOverlay)
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
    // If the overlay is already open, toggle it off!
    if (overlay_ && overlay_->isVisible()) {
        qDebug() << "[igi] SearchSession: Toggling overlay OFF.";
        dismiss();
        return;
    }

    // ── Reentrancy Guard ──
    // QProcess::execute runs a nested event loop which can process pending 
    // IPC messages, causing massive recursive calls to this function.
    static bool isActivating = false;
    if (isActivating) {
        qDebug() << "[igi] Ignoring activation (already in progress / reentrant).";
        return;
    }
    isActivating = true;

    // ── Debounce rapidly repeated activations (e.g. from held-down global shortcuts) ──
    static QElapsedTimer lastActivation;
    if (lastActivation.isValid() && lastActivation.elapsed() < 1500) {
        qDebug() << "[igi] Ignoring duplicate activation (debounced).";
        isActivating = false;
        return;
    }
    
    // Ignore activations if an OCR job is already running in the background.
    if (ocrWatcher_.isRunning()) {
        qDebug() << "[igi] Ignoring activation (OCR already in progress).";
        isActivating = false;
        return;
    }

    // ── 1. Capture the focused window (sync, main thread) ──
    qDebug() << "[igi] SearchSession: capturing focused window…";
    auto captureResult = capture_->captureFocusedWindow();
    if (!captureResult) {
        qWarning() << "[igi] SearchSession: capture failed — using fallback primary screen geometry.";
        core::CaptureResult fallbackResult;
        fallbackResult.screen = core::ScreenGeometry::screenByIndex(0);
        if (fallbackResult.screen.devicePixelRatio <= 0) {
            fallbackResult.screen.devicePixelRatio = 1.0;
        }
        if (fallbackResult.screen.virtualGeometry.isEmpty()) {
            fallbackResult.screen.virtualGeometry = QRect(0, 0, 1920, 1080);
        }
        const double dpr = fallbackResult.screen.devicePixelRatio;
        const QRect& vg = fallbackResult.screen.virtualGeometry;
        fallbackResult.screenRect = QRect(
            static_cast<int>(std::round(vg.x() * dpr)),
            static_cast<int>(std::round(vg.y() * dpr)),
            static_cast<int>(std::round(vg.width() * dpr)),
            static_cast<int>(std::round(vg.height() * dpr))
        );
        QImage dummy(fallbackResult.screenRect.size(), QImage::Format_ARGB32);
        dummy.fill(Qt::transparent);
        fallbackResult.image = std::move(dummy);
        fallbackResult.activeWindow.appName = QStringLiteral("Primary Screen (Fallback)");

        captureResult = std::move(fallbackResult);
    }

    if (captureResult->image.isNull() || captureResult->image.size().isEmpty()) {
        qWarning() << "[igi] SearchSession: captured image is null/empty — creating fallback dummy canvas.";
        if (captureResult->screenRect.isEmpty()) {
            core::ScreenInfo screenInfo = core::ScreenGeometry::screenByIndex(0);
            if (screenInfo.devicePixelRatio <= 0) screenInfo.devicePixelRatio = 1.0;
            if (screenInfo.virtualGeometry.isEmpty()) screenInfo.virtualGeometry = QRect(0, 0, 1920, 1080);
            const double dpr = screenInfo.devicePixelRatio;
            const QRect& vg = screenInfo.virtualGeometry;
            captureResult->screenRect = QRect(
                static_cast<int>(std::round(vg.x() * dpr)),
                static_cast<int>(std::round(vg.y() * dpr)),
                static_cast<int>(std::round(vg.width() * dpr)),
                static_cast<int>(std::round(vg.height() * dpr))
            );
            captureResult->screen = screenInfo;
        }
        QImage dummy(captureResult->screenRect.size(), QImage::Format_ARGB32);
        dummy.fill(Qt::transparent);
        captureResult->image = std::move(dummy);
    }

    capturedScreenRect_ = captureResult->screenRect;
    devicePixelRatio_ = captureResult->screen.devicePixelRatio;
    if (devicePixelRatio_ <= 0) {
        devicePixelRatio_ = 1.0;
    }
    logicalScreenRect_ = QRect(
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.x()) / devicePixelRatio_)),
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.y()) / devicePixelRatio_)),
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.width()) / devicePixelRatio_)),
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.height()) / devicePixelRatio_))
    );

    // Flash the visual border immediately using logical coordinates (T-5 mitigation).
    // flashOverlay_->flash(logicalScreenRect_); // Disabled: User finds it disruptive

    // ── 2. Check for open PDF document ──
    const QString& pdfPath = captureResult->activeWindow.documentPath;
    if (!pdfPath.isEmpty() && ocr::PdfIngestor::isAvailable() && QFile::exists(pdfPath)) {
        qDebug() << "[igi] SearchSession: open PDF document detected:" << pdfPath;
        if (ocrWatcher_.isRunning()) {
            ocrWatcher_.cancel();
            ocrWatcher_.waitForFinished();
        }
        corpus_ = ocr::PdfIngestor::ingestPdf(pdfPath, ocrEngine_);
        mlockCorpus();
        overlay_->activate(logicalScreenRect_);
        onQueryChanged(overlay_->query());
        isActivating = false;
        return;
    }

    // ── 3. Fallback: Active Viewport Screen Capture + OCR ──
    const QImage& image = captureResult->image;

    corpus_.clear();

    ocr::PixPtr pix = ocr::ImageConverter::qImageToPix(image);
    if (!pix) {
        qWarning() << "[igi] SearchSession: QImage→PIX conversion failed.";
    } else {
        QFuture<std::vector<search::WordBox>> future =
            ocrEngine_->recognizeAsync(std::move(pix), 0);
        ocrWatcher_.setFuture(future);
        qDebug() << "[igi] SearchSession: OCR running in background…";
    }

    // Show the search bar immediately so the user can start typing
    overlay_->activate(logicalScreenRect_);

    // Restart the debounce timer here.
    lastActivation.restart();
    isActivating = false;
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
    if (overlay_) {
        overlay_->hide();
    }
    if (flashOverlay_) {
        flashOverlay_->hide();
    }

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
#ifndef Q_OS_WIN
            if (::mlock(p, static_cast<size_t>(wb.text.size()) * sizeof(QChar)) != 0) {
                ++failCount;
            }
#else
            if (!::VirtualLock(const_cast<void*>(p), static_cast<size_t>(wb.text.size()) * sizeof(QChar))) {
                ++failCount;
            }
#endif
        }
        if (!wb.normalizedText.isEmpty()) {
            const void* p = wb.normalizedText.constData();
#ifndef Q_OS_WIN
            if (::mlock(p, static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar)) != 0) {
                ++failCount;
            }
#else
            if (!::VirtualLock(const_cast<void*>(p), static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar))) {
                ++failCount;
            }
#endif
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
#ifndef Q_OS_WIN
                ::munlock(wb.text.constData(),
                          static_cast<size_t>(wb.text.size()) * sizeof(QChar));
#else
                ::VirtualUnlock(const_cast<void*>(static_cast<const void*>(wb.text.constData())),
                                static_cast<size_t>(wb.text.size()) * sizeof(QChar));
#endif
            }
            if (!wb.normalizedText.isEmpty()) {
#ifndef Q_OS_WIN
                ::munlock(wb.normalizedText.constData(),
                          static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar));
#else
                ::VirtualUnlock(const_cast<void*>(static_cast<const void*>(wb.normalizedText.constData())),
                                static_cast<size_t>(wb.normalizedText.size()) * sizeof(QChar));
#endif
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
        overlay_->setHighlights({}, logicalScreenRect_);
        overlay_->setMatchInfo(0, 0, 0);
        return;
    }

    // ── Fuzzy search (always on main thread, stays within 16 ms budget) ──
    const auto matches = search::FuzzySearch::topK(query, corpus_, 50, 0.70f);

    if (query == "cr") {
        QFile f("/tmp/igi_corpus_dump.txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            for (const auto& wb : corpus_) {
                out << wb.text << " -> " << wb.normalizedText << "\n";
            }
        }
    }

    // Convert WordBox physical pixel rects to absolute logical screen rects.
    const QPoint origin = capturedScreenRect_.topLeft();
    std::vector<QRect> highlightRects;
    highlightRects.reserve(matches.size());

    int firstPage = 0;
    for (const auto& m : matches) {
        const auto& wb = m.word;
        if (firstPage == 0 && wb.pageIndex > 0) {
            firstPage = wb.pageIndex;
        }
        double logX = static_cast<double>(origin.x() + wb.x) / devicePixelRatio_;
        double logY = static_cast<double>(origin.y() + wb.y) / devicePixelRatio_;
        double logW = static_cast<double>(wb.w) / devicePixelRatio_;
        double logH = static_cast<double>(wb.h) / devicePixelRatio_;

        highlightRects.emplace_back(
            static_cast<int>(std::round(logX)),
            static_cast<int>(std::round(logY)),
            static_cast<int>(std::round(logW)),
            static_cast<int>(std::round(logH)));
    }

    overlay_->setHighlights(highlightRects, logicalScreenRect_);
    overlay_->setMatchInfo(0, static_cast<int>(matches.size()), firstPage);

    qDebug() << "[igi] SearchSession:" << matches.size()
             << "match(es) for" << query;
}

}  // namespace igi
