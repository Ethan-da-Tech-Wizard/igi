#pragma once

#include <memory>
#include <vector>

#include <sys/mman.h>   // mlock / munlock

#include <QFutureWatcher>
#include <QObject>
#include <QRect>
#include <QString>

#include "ocr/OcrEngine.h"
#include "search/BoundingBox.h"

namespace igi::core { class IScreenCapture; }
namespace igi::ui   { class SearchOverlay;  }

namespace igi {

// SearchSession orchestrates a single Cmd+Shift+F interaction:
//
//   1. On activate() — grab the focused window's pixels (sync, main thread).
//   2. Feed the QImage through ImageConverter → OcrEngine::recognizeAsync().
//   3. When OCR completes, cache the WordBox corpus in RAM.
//   4. On every queryChanged() from SearchOverlay, run FuzzySearch::topK()
//      and forward the matching screen rects to the overlay.
//   5. On dismiss() or a new activate() call, clear everything (RAII).
//
// The session object is reused across invocations.  All sensitive data
// is held only in-scope; dismissal drops the corpus vector.
class SearchSession : public QObject {
    Q_OBJECT
public:
    explicit SearchSession(
        core::IScreenCapture*  capture,
        ocr::OcrEngine*        ocrEngine,
        ui::SearchOverlay*     overlay,
        QObject* parent = nullptr);

    // Trigger a new capture+OCR cycle.  Safe to call from the main thread.
    void activate();

    // Tear down the current session: drop the corpus, hide the overlay.
    void dismiss();

private slots:
    void onOcrFinished();
    void onQueryChanged(const QString& query);

private:
    core::IScreenCapture*   capture_;
    ocr::OcrEngine*         ocrEngine_;
    ui::SearchOverlay*      overlay_;

    // Physical-pixel rect of the most-recently captured window.
    QRect capturedScreenRect_;

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
