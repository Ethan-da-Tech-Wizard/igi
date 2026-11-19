// ==============================================================================
// Program: ScreenCapture.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Captures targeted window and display pixels using native OS APIs (macOS CoreGraphics and Qt QScreen), handling display scaling factors and multi-monitor layouts.
// ==============================================================================

#pragma once

#include <memory>
#include <optional>

#include <QImage>
#include <QRect>

#include "core/ActiveWindowInspector.h"
#include "core/ScreenGeometry.h"

namespace igi::core {
/**
 * What one capture returns. Everything SearchSession needs to run OCR and map
 * results back to the screen.
 */
struct CaptureResult {
    // The captured pixels.
    QImage           image;        // Format_ARGB32, ready for ImageConverter
    QRect            screenRect;   // physical-pixel rect of the captured window
    ScreenInfo       screen;       // which display the window lives on
    ActiveWindowInfo activeWindow; // active window metadata including documentPath
};

// Abstraction over the OS screen-capture API, so tests can substitute a mock.
// Implementations: ScreenCapture.mm (macOS, CGWindowListCreateImage; a future
// ScreenCaptureKit version for macOS 14.4+ is possible) and
// ScreenCapture_qt.cpp (Qt / Wayland fallbacks).
// Pure interface; callers own the object through std::unique_ptr.
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;

    /**
     * Captures the currently focused window's pixels.
     * @return nullopt if no active window is detectable, the window is off-screen
     *         or the capture API fails (the Qt implementation never returns nullopt).
     * Must be called on the main thread, not a worker (the CG window list API is
     * not thread-safe).
     */
    virtual std::optional<CaptureResult> captureFocusedWindow() = 0;

    // Platform factory. Implemented in ScreenCapture.mm on Apple, ScreenCapture_qt.cpp elsewhere.
    static std::unique_ptr<IScreenCapture> create();
};

}  // namespace igi::core
