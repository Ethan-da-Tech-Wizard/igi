// ==============================================================================
// Program: SearchSession.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Coordinates the complete screen search workflow lifecycle, orchestrating target window capture, background OCR processing, fuzzy query evaluation, and overlay display.
//
// How it fits: main.cpp creates one SearchSession and connects the Daemon's
// hotkeyTriggered signal to activate(). The session is the only object that
// talks to all the others: IScreenCapture (screenshot + active-window info),
// ImageConverter/OcrEngine (Tesseract on a background thread), PdfIngestor
// (reads text straight from an open PDF), FuzzySearch (ranking) and the
// SearchOverlay / CaptureFlashOverlay windows.
//
// Data flow for one hotkey press:
//   activate() -> capture window -> [PDF open? ingest text : OCR async] ->
//   corpus_ (vector of WordBox: word text + pixel rectangle) ->
//   onQueryChanged() on every keystroke -> FuzzySearch::topK -> highlight rects.
//
// Coordinates: the capture is in PHYSICAL pixels; the overlay draws in LOGICAL
// (device-independent) pixels. devicePixelRatio_ converts between them
// (logical = physical / ratio) on Retina/HiDPI screens.
//
// Security design (see docs/RISK_REGISTER.md SEC-01/SEC-02, D-006): OCR text can
// contain sensitive data, so the corpus is pinned in RAM with mlock() (no
// swap), and dismiss() overwrites the text buffers with igi_bzero() before
// releasing them. See the NOTE in dismiss() about copy-on-write limits.
//
// Known limits: the "cr" debug dump in onQueryChanged() (see there) defeats
// the wipe; activate() uses function-local statics for re-entrancy and debounce,
// so there is effectively one session per process.
// ==============================================================================

#include "SearchSession.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// igi_bzero: a zeroing routine the compiler may not optimise away (unlike a plain memset).
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

/**
 * Stores the collaborators (borrowed, not owned) and connects the three
 * signals the session reacts to. The collaborators must outlive the session.
 * @param capture      Screenshot provider.
 * @param ocrEngine    Initialised OCR engine.
 * @param overlay      Search bar + highlight window.
 * @param flashOverlay Border flash window (currently disabled in activate()).
 * @param parent       Optional QObject owner.
 */
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
    // OCR finished (background thread -> main thread, via the watcher).
    connect(&ocrWatcher_, &QFutureWatcher<std::vector<search::WordBox>>::finished,
            this, &SearchSession::onOcrFinished);

    // Every keystroke in the search box re-runs the fuzzy search.
    connect(overlay_, &ui::SearchOverlay::queryChanged,
            this, &SearchSession::onQueryChanged);

    // Escape in the overlay tears the session down and wipes the corpus.
    connect(overlay_, &ui::SearchOverlay::dismissed,
            this, &SearchSession::dismiss);
}

/**
 * Hotkey handler: starts one capture + OCR cycle, or toggles the overlay off
 * if it is already showing. Must run on the main (GUI) thread.
 *
 * Ignored (silently) when: a previous activation is still being handled
 * (re-entrancy), a second activation arrives within 1.5 s of the previous
 * completed one (debounce), or an OCR job is still running.
 */
void SearchSession::activate() {
    // The hotkey is a toggle: pressing it again while the overlay is open closes it.
    if (overlay_ && overlay_->isVisible()) {
        qDebug() << "[igi] SearchSession: Toggling overlay OFF.";
        dismiss();
        return;
    }

    // Re-entrancy guard. Capturing may run a nested event loop (e.g. QProcess::execute of
    // spectacle/grim in the Qt capture backend) which can deliver another queued hotkey/IPC
    // message into this very function.
    static bool isActivating = false;
    if (isActivating) {
        qDebug() << "[igi] Ignoring activation (already in progress / reentrant).";
        return;
    }
    isActivating = true;

    // Debounce: held-down or double-pressed hotkeys would otherwise start two
    // captures. 1.5 s is longer than a typical key-repeat burst. The timer is
    // (re)started only at the end of a full capture+OCR start.
    static QElapsedTimer lastActivation;
    if (lastActivation.isValid() && lastActivation.elapsed() < 1500) {
        qDebug() << "[igi] Ignoring duplicate activation (debounced).";
        isActivating = false;
        return;
    }
    
    // A new OCR job on top of a running one would race for the same result slot.
    if (ocrWatcher_.isRunning()) {
        qDebug() << "[igi] Ignoring activation (OCR already in progress).";
        isActivating = false;
        return;
    }

    // Step 1: capture the focused window synchronously on the main thread (the
    // OS capture APIs require it).
    qDebug() << "[igi] SearchSession: capturing focused window…";
    auto captureResult = capture_->captureFocusedWindow();
    if (!captureResult) {
        // Capture failed (e.g. no Screen Recording permission): fall back to a blank
        // canvas the size of the primary screen so the search bar still appears.
        // Defaults of 1.0 DPR and 1920x1080 cover screens that report nothing.
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
        // Physical-pixel rect = logical geometry x device pixel ratio.
        fallbackResult.screenRect = QRect(
            static_cast<int>(std::round(vg.x() * dpr)),
            static_cast<int>(std::round(vg.y() * dpr)),
            static_cast<int>(std::round(vg.width() * dpr)),
            static_cast<int>(std::round(vg.height() * dpr))
        );
        // A transparent dummy image: OCR finds no words, the overlay still works.
        QImage dummy(fallbackResult.screenRect.size(), QImage::Format_ARGB32);
        dummy.fill(Qt::transparent);
        fallbackResult.image = std::move(dummy);
        fallbackResult.activeWindow.appName = QStringLiteral("Primary Screen (Fallback)");

        captureResult = std::move(fallbackResult);
    }

    // Capture succeeded but returned an empty image: same fallback, reusing the
    // capture's own geometry when it has any.
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

    // Remember the geometry for later conversions in onQueryChanged().
    capturedScreenRect_ = captureResult->screenRect;
    devicePixelRatio_ = captureResult->screen.devicePixelRatio;
    if (devicePixelRatio_ <= 0) {
        devicePixelRatio_ = 1.0;
    }
    // Inverse of the physical rect above: logical = physical / ratio (rounded).
    logicalScreenRect_ = QRect(
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.x()) / devicePixelRatio_)),
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.y()) / devicePixelRatio_)),
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.width()) / devicePixelRatio_)),
        static_cast<int>(std::round(static_cast<double>(capturedScreenRect_.height()) / devicePixelRatio_))
    );

    // Border flash (threat T-5 mitigation: show the user which area is being
    // read). Disabled because it was found disruptive.
    // flashOverlay_->flash(logicalScreenRect_); // Disabled: User finds it disruptive

    // Step 2: if the active window has an open PDF, read its text directly; that
    // is faster and more accurate than OCR on a screenshot.
    const QString& pdfPath = captureResult->activeWindow.documentPath;
    if (!pdfPath.isEmpty() && ocr::PdfIngestor::isAvailable() && QFile::exists(pdfPath)) {
        qDebug() << "[igi] SearchSession: open PDF document detected:" << pdfPath;
        // Cancel any stale OCR so it cannot overwrite the PDF corpus later.
        if (ocrWatcher_.isRunning()) {
            ocrWatcher_.cancel();
            ocrWatcher_.waitForFinished();
        }
        // The PDF path produces the corpus synchronously; no OCR watcher is involved.
        corpus_ = ocr::PdfIngestor::ingestPdf(pdfPath, ocrEngine_);
        // Pin the pages (see mlockCorpus) then show the overlay and apply any
        // query the user already typed. Note that the debounce timer is not
        // restarted on this path.
        mlockCorpus();
        overlay_->activate(logicalScreenRect_);
        onQueryChanged(overlay_->query());
        isActivating = false;
        return;
    }

    // Step 3: fallback (no PDF): OCR the captured pixels in the background.
    const QImage& image = captureResult->image;

    // The previous corpus was already wiped by dismiss() (the overlay can only be
    // hidden through it); this clear() just guarantees the vector is empty.
    corpus_.clear();

    // Tesseract works on Leptonica PIX images; convert once on the main thread.
    ocr::PixPtr pix = ocr::ImageConverter::qImageToPix(image);
    if (!pix) {
        qWarning() << "[igi] SearchSession: QImage→PIX conversion failed.";
    } else {
        // The result is delivered to onOcrFinished() through ocrWatcher_ on the main thread.
        QFuture<std::vector<search::WordBox>> future =
            ocrEngine_->recognizeAsync(std::move(pix), 0);
        ocrWatcher_.setFuture(future);
        qDebug() << "[igi] SearchSession: OCR running in background…";
    }

    // Show the search bar immediately so the user can start typing while OCR runs.
    overlay_->activate(logicalScreenRect_);

    // Start the debounce window now that the capture is underway.
    lastActivation.restart();
    isActivating = false;
}

/**
 * Ends the session: wipes and releases the OCR corpus, then hides both overlays.
 * Called by Escape in the overlay, by the toggle in activate(), and by main()
 * on every exit path (signals, tray Quit, normal exit). Safe to call when idle.
 */
void SearchSession::dismiss() {
    // Secure corpus wipe (D-006 / SEC-02).
    // corpus_.clear() alone only returns the QString buffers to the allocator; the
    // text (potentially sensitive) stays readable in freed memory until reused.
    // So each WordBox string is detached (data()) and overwritten with igi_bzero(),
    // which the compiler may not remove, before the vector is cleared.
    // NOTE: QString is copy-on-write. If another copy shares the buffer (for example
    // the QFutureWatcher's stored result), data() detaches and only the NEW private
    // copy is wiped, so the original may survive. munlockAndWipeCorpus() below
    // repeats the same pass.
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

    // Second pass plus munlock and clear (see below).
    munlockAndWipeCorpus();
    // Forget the geometry so stale coordinates cannot be reused.
    capturedScreenRect_ = {};
    if (overlay_) {
        overlay_->hide();
    }
    if (flashOverlay_) {
        flashOverlay_->hide();
    }

    qDebug() << "[igi] SearchSession: dismissed — corpus securely wiped.";
}

// mlock helpers. SEC-01 (RISK_REGISTER.md): mlock() pins pages in RAM so the
// OS cannot write them to swap, where a later wipe could not reach them.
// Each QString's heap buffer is locked separately because std::vector<WordBox>
// stores the WordBox objects contiguously but the UTF-16 payload of each of its
// two QStrings lives in its own allocation. mlock() is limited by
// RLIMIT_MEMLOCK; on failure we log a warning and carry on, with the
// bzero wipe on dismiss() as the remaining control.
/**
 * Pins every word buffer in the corpus in RAM (mlock on POSIX, VirtualLock on
 * Windows) and records that it did so in corpusLocked_. Failures are counted
 * and logged, never fatal.
 */
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

/**
 * Zeroes every word buffer, unlocks the pages (only if they were locked), then
 * releases the vector's storage. Order matters: wipe BEFORE unlock, so a page
 * cannot be swapped out between unlocking and zeroing.
 */
void SearchSession::munlockAndWipeCorpus() {
    // Step 1: overwrite the text. data() detaches a shared QString (see the NOTE in dismiss()).
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

    // Step 2: unlock after the wipe. Unlocking first would, in theory, let the OS
    // page out the unzeroed data under heavy memory pressure.
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

    // Step 3: release the vector (shrink_to_fit actually returns the capacity).
    // ── Step 3: release the vector ──
    corpus_.clear();
    corpus_.shrink_to_fit();
}



/**
 * Slot: the background OCR finished (runs on the main thread).
 * Takes the result, pins it in RAM before any search can happen (SEC-01), and
 * re-runs the search with whatever the user typed while OCR was running.
 */
void SearchSession::onOcrFinished() {
    corpus_ = ocrWatcher_.result();
    qDebug() << "[igi] SearchSession: OCR complete," << corpus_.size() << "words extracted.";

    mlockCorpus();

    onQueryChanged(overlay_->query());
}


/**
 * Slot: the search text changed. Runs FuzzySearch::topK on the main thread
 * (cheap enough to stay within about one frame, 16 ms) and gives the overlay
 * highlight rectangles and match counts.
 * @param query Current text of the search bar; empty or blank clears the highlights.
 */
void SearchSession::onQueryChanged(const QString& query) {
    if (corpus_.empty() || query.trimmed().isEmpty()) {
        // No corpus yet or empty query: clear highlights.
        // Nothing to highlight yet: clear the overlay.
        overlay_->setHighlights({}, logicalScreenRect_);
        overlay_->setMatchInfo(0, 0, 0);
        return;
    }

    // At most 50 matches with a minimum score of 0.70 (see FuzzySearch.h for the score).
    const auto matches = search::FuzzySearch::topK(query, corpus_, 50, 0.70f);

    // WARNING (debug code left in): when the query is exactly "cr" the whole OCR
    // corpus is written in plain text to /tmp/igi_corpus_dump.txt. This bypasses
    // the wipe and mlock protections above; it should be removed (see
    // test_dump_corpus.patch, which is where it came from).
    if (query == "cr") {
        QFile f("/tmp/igi_corpus_dump.txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            for (const auto& wb : corpus_) {
                out << wb.text << " -> " << wb.normalizedText << "\n";
            }
        }
    }

    // Convert WordBox pixel rectangles (relative to the captured window, physical
    // pixels) to absolute logical screen coordinates: add the window origin, then
    // divide by the device pixel ratio.
    const QPoint origin = capturedScreenRect_.topLeft();
    std::vector<QRect> highlightRects;
    highlightRects.reserve(matches.size());

    int firstPage = 0;
    for (const auto& m : matches) {
        const auto& wb = m.word;
        // Remember the first page number (PDF input only; OCR words have pageIndex 0).
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

    // Current-match index 0 for now; the overlay shows "n of total" and the page.
    overlay_->setHighlights(highlightRects, logicalScreenRect_);
    overlay_->setMatchInfo(0, static_cast<int>(matches.size()), firstPage);

    qDebug() << "[igi] SearchSession:" << matches.size()
             << "match(es) for" << query;
}

}  // namespace igi
