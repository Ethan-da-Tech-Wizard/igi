// ==============================================================================
// Program: SearchSession.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Coordinates the complete screen search workflow lifecycle, orchestrating target window capture, background OCR processing, fuzzy query evaluation, and overlay display.
// ==============================================================================

#pragma once

#include <memory>
#include <vector>

#ifndef Q_OS_WIN
#include <sys/mman.h>   // mlock / munlock
#endif

#include <QFutureWatcher>
#include <QObject>
#include <QRect>
#include <QString>

#include "ocr/OcrEngine.h"
#include "search/BoundingBox.h"

namespace igi::core { class IScreenCapture; }
namespace igi::ui   { 
    class SearchOverlay;
    class CaptureFlashOverlay;
}

namespace igi {

// SearchSession orchestrates a single hotkey interaction (Cmd+Ctrl+F on macOS,
// Meta+Ctrl+F elsewhere; see PlatformHotkey.h):
//
//   1. On activate() — grab the focused window's pixels (sync, main thread).
//   2. Feed the QImage through ImageConverter → OcrEngine::recognizeAsync().
//   3. When OCR completes, cache the WordBox corpus in RAM.
//   4. On every queryChanged() from SearchOverlay, run FuzzySearch::topK()
//      and forward the matching screen rects to the overlay.
//   5. On dismiss() or a new activate() call, clear everything (RAII).
//
// The session object is reused across invocations.  All sensitive data
// is held only in-scope; dismissal overwrites the corpus text and then drops the
// corpus vector (see SearchSession::dismiss for the wipe and its limits).
class SearchSession : public QObject {
    Q_OBJECT
public:
    explicit SearchSession(
        core::IScreenCapture*  capture,
        ocr::OcrEngine*        ocrEngine,
        ui::SearchOverlay*     overlay,
        ui::CaptureFlashOverlay* flashOverlay,
        QObject* parent = nullptr);

    // Trigger a new capture+OCR cycle (or toggle the overlay off if it is open).
    // Must be called on the main (GUI) thread; connected to Daemon::hotkeyTriggered
    // and to the single-instance IPC "ACTIVATE" message in main.cpp.
    void activate();

    // Tear down the current session: securely wipe and drop the corpus, hide both overlays.
    void dismiss();

private slots:
    // Background OCR finished: take the result as the corpus and re-run the search.
    void onOcrFinished();
    // The search text changed: run the fuzzy search and update the highlights.
    void onQueryChanged(const QString& query);

private:
    // Collaborators, borrowed (not owned); main() keeps them alive for the whole run.
    core::IScreenCapture*   capture_;
    ocr::OcrEngine*         ocrEngine_;
    ui::SearchOverlay*      overlay_;
    ui::CaptureFlashOverlay* flashOverlay_;

    // Physical-pixel rect of the most-recently captured window.
    QRect capturedScreenRect_;
    
    // Logical-pixel rect of the window, used for Qt rendering.
    QRect logicalScreenRect_;
    
    // The device pixel ratio of the screen the window was captured on.
    double devicePixelRatio_ = 1.0;

    // The OCR corpus for the current session. Cleared on dismiss().
    std::vector<search::WordBox> corpus_;

    // Watcher for the async OCR QFuture.
    QFutureWatcher<std::vector<search::WordBox>> ocrWatcher_;

    // Tracks whether the corpus pages are currently mlock()-ed.
    bool corpusLocked_ = false;

    // Lock all QString heap buffers in the corpus against OS swap (D-002/SEC-01).
    // Called immediately after OCR results arrive.
    void mlockCorpus();

    // Unlock and zero the corpus pages before releasing them (dismiss).
    void munlockAndWipeCorpus();
};

}  // namespace igi
