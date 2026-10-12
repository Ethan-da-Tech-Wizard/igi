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

namespace {

QScreen* findBestScreen(const QRect& bounds) {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return nullptr;
    }

    // 1. Try screen containing center of bounds
    QPoint center = bounds.center();
    for (QScreen* s : screens) {
        if (s && s->geometry().contains(center)) {
            return s;
        }
    }

    // 2. Try screen containing top-left of bounds
    for (QScreen* s : screens) {
        if (s && s->geometry().contains(bounds.topLeft())) {
            return s;
        }
    }

    // 3. Find screen with maximum intersection area
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

    // 4. Fallback to primary screen (or first available screen)
    QScreen* primary = QGuiApplication::primaryScreen();
    if (!primary && !screens.isEmpty()) {
        primary = screens.first();
    }
    return primary;
}

} // namespace

class QtScreenCapture final : public IScreenCapture {
public:
    std::optional<CaptureResult> captureFocusedWindow() override {
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

        double dpr = screen.devicePixelRatio;
        if (dpr <= 0.0) {
            dpr = 1.0;
        }

        QRect screenGeo = (qScreen ? qScreen->geometry() : QRect(0, 0, 1920, 1080));
        screen.virtualGeometry = screenGeo;

        // Clamp bounds strictly within screen geometry
        QRect clampedBounds = bounds.intersected(screenGeo);
        if (clampedBounds.isEmpty() || clampedBounds.width() <= 0 || clampedBounds.height() <= 0) {
            clampedBounds = screenGeo;
        }

        int localX = clampedBounds.x() - screenGeo.x();
        int localY = clampedBounds.y() - screenGeo.y();
        int localW = clampedBounds.width();
        int localH = clampedBounds.height();

        // Ensure local coordinates are strictly within screen boundaries
        localX = std::clamp(localX, 0, std::max(0, screenGeo.width() - 1));
        localY = std::clamp(localY, 0, std::max(0, screenGeo.height() - 1));
        localW = std::clamp(localW, 1, screenGeo.width() - localX);
        localH = std::clamp(localH, 1, screenGeo.height() - localY);

        QPixmap pixmap;

        // Stage 1: Try sub-rectangle grab on target screen
        if (qScreen) {
            pixmap = qScreen->grabWindow(0, localX, localY, localW, localH);
        }

        // Stage 2: Fallback to full screen grab on target screen if sub-rectangle grab returned null
        if (pixmap.isNull() && qScreen) {
            qDebug() << "[igi] ScreenCapture: Sub-rectangle grabWindow returned null. Trying full screen grabWindow(0).";
            pixmap = qScreen->grabWindow(0);
            if (!pixmap.isNull()) {
                // Crop target sub-rectangle from full screen pixmap
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

        // Stage 3: Fallback to iterating all available screens if target screen grab failed
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
        
        // Stage 3.5: Wayland Fallback using CLI tools (spectacle, grim)
        if (pixmap.isNull()) {
            qDebug() << "[igi] ScreenCapture: All grabWindow attempts returned null image. Trying Wayland CLI fallbacks (spectacle/grim).";
            
            QTemporaryFile tempFile;
            tempFile.setFileTemplate(QDir::tempPath() + "/igi_capture_XXXXXX.png");
            if (tempFile.open()) {
                QString tempPath = tempFile.fileName();
                tempFile.close(); // let CLI tool write to it
                
                int exitCode = -1;
                // Try KDE spectacle first (-m captures the current monitor containing cursor)
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
                QFile::remove(tempPath);
            }
        }

        if (!pixmap.isNull() && image.isNull()) {
            image = pixmap.toImage();
        }

        // Stage 4: Synthetic Fallback ARGB32 image so screen capture NEVER fails or returns null
        if (image.isNull()) {
            qWarning() << "[igi] ScreenCapture: All grabWindow attempts returned null image. Generating fallback synthetic QImage.";
            int imgW = std::max(1, clampedBounds.width());
            int imgH = std::max(1, clampedBounds.height());
            int physW = std::max(1, static_cast<int>(std::round(imgW * dpr)));
            int physH = std::max(1, static_cast<int>(std::round(imgH * dpr)));
            image = QImage(physW, physH, QImage::Format_ARGB32);
            image.fill(QColor(30, 30, 30));
        } else if (image.format() != QImage::Format_ARGB32) {
            image = image.convertToFormat(QImage::Format_ARGB32);
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
};

std::unique_ptr<IScreenCapture> IScreenCapture::create() {
    return std::make_unique<QtScreenCapture>();
}

} // namespace igi::core

