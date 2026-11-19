// ==============================================================================
// Program: ScreenCapture_qt.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Captures targeted window and display pixels using native OS APIs (macOS CoreGraphics and Qt QScreen), handling display scaling factors and multi-monitor layouts.
//
// How it fits: this is the non-macOS implementation of IScreenCapture (the
// macOS one is ScreenCapture.mm). It is selected at build time by CMake and
// returned by IScreenCapture::create(); SearchSession calls
// captureFocusedWindow() on every hotkey press. It also uses
// queryActiveWindow() (ActiveWindowInspector_*.cpp) to find the window to
// capture and ScreenGeometry for per-screen scale factors.
//
// Strategy: a chain of fallbacks so a capture is ALWAYS returned (a screenshot
// failure must never stop the search bar from appearing):
//   1. grab the window's rectangle with QScreen::grabWindow
//   2. grab the whole screen and crop
//   3. try every other screen
//   3.5 external tools (spectacle, grim, gnome-screenshot) for Wayland, where
//       Qt may not be allowed to grab the screen
//   4. a dark synthetic image of the right size
//
// Coordinates: Qt screen/window rectangles are LOGICAL pixels; the returned
// CaptureResult::screenRect is PHYSICAL pixels (logical x devicePixelRatio).
//
// Privacy: a temporary PNG written by the external tools is deleted right
// after loading; QTemporaryFile creates it with owner-only permissions.
// ==============================================================================

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include "core/ScreenCapture.h"
#include "core/ActiveWindowInspector.h"
#include "core/ScreenGeometry.h"

#include <algorithm>
#include <cmath>
#include <QColor>
#include <QDebug>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QProcess>
#include <QTemporaryFile>

namespace igi::core {

// Helpers private to this file.
namespace {

/**
 * Picks the screen a window mostly belongs to.
 * Preference order: the screen containing the window's centre, then its
 * top-left corner, then the screen with the largest overlap, then the primary
 * (or first) screen.
 * @param bounds Window rectangle in logical global coordinates.
 * @return A screen, or nullptr when there are no screens at all.
 */
QScreen* findBestScreen(const QRect& bounds) {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return nullptr;
    }

    QPoint center = bounds.center();
    for (QScreen* s : screens) {
        if (s && s->geometry().contains(center)) {
            return s;
        }
    }

    for (QScreen* s : screens) {
        if (s && s->geometry().contains(bounds.topLeft())) {
            return s;
        }
    }

    QScreen* bestScreen = nullptr;
    int maxArea = 0;
    for (QScreen* s : screens) {
        if (s) {
            QRect intersect = s->geometry().intersected(bounds);
            int area = intersect.width() * intersect.height();
            if (area > maxArea) {
                maxArea = area;
                bestScreen = s;
            }
        }
    }
    if (bestScreen) {
        return bestScreen;
    }

    QScreen* primary = QGuiApplication::primaryScreen();
    if (!primary && !screens.isEmpty()) {
        primary = screens.first();
    }
    return primary;
}

} // namespace

/**
 * Qt-based IScreenCapture (Linux / Windows / Wayland). Stateless; each call
 * does the whole capture again.
 */
class QtScreenCapture final : public IScreenCapture {
public:
    /**
     * Captures the focused window (or the primary screen if the window cannot be
     * determined).
     * @return Always a value in practice: the grabbed image converted to ARGB32,
     *         its physical-pixel rectangle, the screen's metadata and the active
     *         window's metadata (name, document path).
     */
    std::optional<CaptureResult> captureFocusedWindow() override {
        // Which window is focused? Not available on Wayland (security model), in which
        // case active is empty and the whole primary screen is used.
        const auto active = queryActiveWindow();

        QScreen* qScreen = QGuiApplication::primaryScreen();
        QRect bounds;
        ActiveWindowInfo windowInfo;

        if (active && !active->bounds.isEmpty()) {
            windowInfo = *active;
            bounds = active->bounds;
        } else {
            // Wayland fallback: capture full primary screen when window inspection is restricted
            qDebug() << "[igi] ScreenCapture: No X11 active window (Wayland session). Falling back to primary screen capture.";
            if (qScreen) {
                bounds = qScreen->geometry();
            } else {
                bounds = QRect(0, 0, 1920, 1080);
            }
            windowInfo.bounds = bounds;
            windowInfo.appName = QStringLiteral("Primary Screen (Wayland)");
        }

        // Select best matching screen for window bounds
        QScreen* targetScreen = findBestScreen(bounds);
        if (targetScreen) {
            qScreen = targetScreen;
        }

        const auto screens = QGuiApplication::screens();
        int screenIndex = qScreen ? screens.indexOf(qScreen) : -1;
        ScreenInfo screen = ScreenGeometry::screenByIndex(screenIndex);
        if (screen.index < 0 && !screens.isEmpty()) {
            qScreen = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen() : screens.first();
            screenIndex = screens.indexOf(qScreen);
            screen = ScreenGeometry::screenByIndex(screenIndex);
        }

        // A non-positive ratio would zero-size the image below; treat it as 1.
        double dpr = screen.devicePixelRatio;
        if (dpr <= 0.0) {
            dpr = 1.0;
        }

        QRect screenGeo = (qScreen ? qScreen->geometry() : QRect(0, 0, 1920, 1080));
        screen.virtualGeometry = screenGeo;

        // Clamp the bounds into the screen: a window may be partly off-screen, and
        // grabWindow needs a rectangle inside the screen.
        QRect clampedBounds = bounds.intersected(screenGeo);
        if (clampedBounds.isEmpty() || clampedBounds.width() <= 0 || clampedBounds.height() <= 0) {
            clampedBounds = screenGeo;
        }

        // Window position relative to the screen's own top-left (grabWindow uses
        // screen-local coordinates).
        int localX = clampedBounds.x() - screenGeo.x();
        int localY = clampedBounds.y() - screenGeo.y();
        int localW = clampedBounds.width();
        int localH = clampedBounds.height();

        // Ensure local coordinates are strictly within screen boundaries and sizes are
        // at least 1 pixel (std::clamp requires lo <= hi, guaranteed by the lines above).
        localX = std::clamp(localX, 0, std::max(0, screenGeo.width() - 1));
        localY = std::clamp(localY, 0, std::max(0, screenGeo.height() - 1));
        localW = std::clamp(localW, 1, screenGeo.width() - localX);
        localH = std::clamp(localH, 1, screenGeo.height() - localY);

        QPixmap pixmap;

        // Stage 1: sub-rectangle grab of the target screen (window 0 = the whole desktop of that screen).
        if (qScreen) {
            pixmap = qScreen->grabWindow(0, localX, localY, localW, localH);
        }

        // Stage 2: some platforms refuse a sub-rectangle grab; grab the full screen and crop.
        if (pixmap.isNull() && qScreen) {
            qDebug() << "[igi] ScreenCapture: Sub-rectangle grabWindow returned null. Trying full screen grabWindow(0).";
            pixmap = qScreen->grabWindow(0);
            if (!pixmap.isNull()) {
                // Crop target sub-rectangle from the full screen pixmap. The pixmap is in
                // physical pixels, the rectangle in logical ones, so scale by pixmap/screen size.
                qreal scaleX = static_cast<qreal>(pixmap.width()) / static_cast<qreal>(screenGeo.width());
                qreal scaleY = static_cast<qreal>(pixmap.height()) / static_cast<qreal>(screenGeo.height());
                if (scaleX > 0 && scaleY > 0) {
                    int cropX = static_cast<int>(std::round(localX * scaleX));
                    int cropY = static_cast<int>(std::round(localY * scaleY));
                    int cropW = static_cast<int>(std::round(localW * scaleX));
                    int cropH = static_cast<int>(std::round(localH * scaleY));
                    QRect cropRect = QRect(cropX, cropY, cropW, cropH).intersected(pixmap.rect());
                    if (!cropRect.isEmpty() && cropRect != pixmap.rect()) {
                        QPixmap cropped = pixmap.copy(cropRect);
                        if (!cropped.isNull()) {
                            pixmap = cropped;
                        }
                    }
                }
            }
        }

        // Stage 3: the target screen failed; try every other screen and, if one works,
        // switch the geometry bookkeeping to it.
        if (pixmap.isNull()) {
            qDebug() << "[igi] ScreenCapture: Target screen grab failed. Trying all available screens.";
            for (QScreen* s : screens) {
                if (s && s != qScreen) {
                    pixmap = s->grabWindow(0);
                    if (!pixmap.isNull()) {
                        qScreen = s;
                        screenIndex = screens.indexOf(s);
                        screen = ScreenGeometry::screenByIndex(screenIndex);
                        dpr = (screen.devicePixelRatio > 0.0) ? screen.devicePixelRatio : 1.0;
                        clampedBounds = s->geometry();
                        break;
                    }
                }
            }
        }

        QImage image;
        
        // Stage 3.5: external tools. They capture the whole monitor, not the window.
        if (pixmap.isNull()) {
            qDebug() << "[igi] ScreenCapture: All grabWindow attempts returned null image. Trying Wayland CLI fallbacks (spectacle/grim).";
            
            QTemporaryFile tempFile;
            tempFile.setFileTemplate(QDir::tempPath() + "/igi_capture_XXXXXX.png");
            if (tempFile.open()) {
                QString tempPath = tempFile.fileName();
                tempFile.close(); // let CLI tool write to it
                
                int exitCode = -1;
                // Try KDE spectacle first (-m captures the current monitor containing cursor), then grim (wlroots), then GNOME.
                if (QProcess::execute("spectacle", {"-b", "-n", "-m", "-o", tempPath}) == 0) {
                    exitCode = 0;
                } else if (QProcess::execute("grim", {tempPath}) == 0) { // Try grim for wlroots
                    exitCode = 0;
                } else if (QProcess::execute("gnome-screenshot", {"-f", tempPath}) == 0) { // Try GNOME
                    exitCode = 0;
                }
                
                if (exitCode == 0 && QFileInfo::exists(tempPath)) {
                    QImage loadedImage(tempPath);
                    if (!loadedImage.isNull()) {
                        qDebug() << "[igi] ScreenCapture: CLI fallback succeeded using temp file:" << tempPath;
                        image = loadedImage;
                        // The CLI tools capture the entire monitor, so we must reset the bounds
                        // to the full screen geometry to prevent double-shifting coordinates.
                        clampedBounds = screenGeo;
                    }
                }
                // Always delete the temporary screenshot, whether or not it loaded.
                QFile::remove(tempPath);
            }
        }

        if (!pixmap.isNull() && image.isNull()) {
            image = pixmap.toImage();
        }

        // Stage 4: Synthetic Fallback ARGB32 image so screen capture NEVER fails or returns null.
        // A dark grey canvas: OCR will find nothing but the overlay still works.
        if (image.isNull()) {
            qWarning() << "[igi] ScreenCapture: All grabWindow attempts returned null image. Generating fallback synthetic QImage.";
            int imgW = std::max(1, clampedBounds.width());
            // Physical size = logical size x device pixel ratio.
            int imgH = std::max(1, clampedBounds.height());
            int physW = std::max(1, static_cast<int>(std::round(imgW * dpr)));
            int physH = std::max(1, static_cast<int>(std::round(imgH * dpr)));
            // OCR and the overlay expect 32-bit ARGB images (see ImageConverter).
            image = QImage(physW, physH, QImage::Format_ARGB32);
            image.fill(QColor(30, 30, 30));
        } else if (image.format() != QImage::Format_ARGB32) {
            image = image.convertToFormat(QImage::Format_ARGB32);
        // Top-left in physical pixels, used by SearchSession to map OCR boxes back to
        // screen coordinates.
        }

        const QPoint topLeftPx(
            static_cast<int>(std::round(clampedBounds.x() * dpr)),
            static_cast<int>(std::round(clampedBounds.y() * dpr))
        );

        CaptureResult result;
        result.image        = std::move(image);
        result.screenRect   = QRect(topLeftPx, result.image.size());
        result.screen       = screen;
        result.activeWindow = windowInfo;
        return result;
    }
/** Factory: the Qt implementation is the platform default on non-Apple builds. */
};

std::unique_ptr<IScreenCapture> IScreenCapture::create() {
    return std::make_unique<QtScreenCapture>();
}

} // namespace igi::core

