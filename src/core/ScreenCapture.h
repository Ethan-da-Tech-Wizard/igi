#pragma once

#include <memory>
#include <optional>

#include <QImage>
#include <QRect>

#include "core/ScreenGeometry.h"

namespace igi::core {

struct CaptureResult {
    QImage     image;        // Format_ARGB32, ready for ImageConverter
    QRect      screenRect;   // physical-pixel rect of the captured window
    ScreenInfo screen;       // which display the window lives on
};

// Abstraction over the OS screen-capture API. Tests use a mock; the
// production factory returns the macOS implementation backed by
// CGWindowListCreateImage. A future Chunk 7 patch may swap the macOS
// impl for ScreenCaptureKit on macOS 14.4+.
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;

    // Capture the currently focused window's pixels. Returns nullopt if
    // no active window is detectable, the window is off-screen, or the
    // capture API fails. Must be safe to call from the main thread; do
    // not call from a worker thread (CG window list is not thread-safe).
    virtual std::optional<CaptureResult> captureFocusedWindow() = 0;

    // Platform factory. Implemented in ScreenCapture.mm on Apple.
    static std::unique_ptr<IScreenCapture> create();
};

}  // namespace igi::core
