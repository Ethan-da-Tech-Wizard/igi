#include "core/ScreenCapture.h"
#include "core/ActiveWindowInspector.h"
#include "core/ScreenGeometry.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>

namespace igi::core {

class QtScreenCapture final : public IScreenCapture {
public:
    std::optional<CaptureResult> captureFocusedWindow() override {
        const auto active = queryActiveWindow();
        if (!active) {
            return std::nullopt;
        }

        const ScreenInfo screen = ScreenGeometry::screenAt(active->bounds.topLeft());
        if (screen.index < 0) {
            return std::nullopt;
        }

        const auto screens = QGuiApplication::screens();
        if (screen.index >= screens.size()) {
            return std::nullopt;
        }

        QScreen* qScreen = screens[screen.index];

        // Convert virtual desktop bounds to screen-local logical coordinates
        int localX = active->bounds.x() - screen.virtualGeometry.x();
        int localY = active->bounds.y() - screen.virtualGeometry.y();

        // Grab window pixel buffer using Qt's platform-agnostic grabWindow API
        QPixmap pixmap = qScreen->grabWindow(0, localX, localY, active->bounds.width(), active->bounds.height());
        QImage image = pixmap.toImage();
        if (image.isNull()) {
            return std::nullopt;
        }

        // Standardise target format to Format_ARGB32 (required by ImageConverter)
        if (image.format() != QImage::Format_ARGB32) {
            image = image.convertToFormat(QImage::Format_ARGB32);
        }

        // Translate virtual-point coordinates to physical-pixel coordinates based on screen DPR
        const double dpr = screen.devicePixelRatio;
        const QPoint topLeftPx(
            static_cast<int>(active->bounds.x() * dpr),
            static_cast<int>(active->bounds.y() * dpr)
        );

        CaptureResult result;
        result.image      = std::move(image);
        result.screenRect = QRect(topLeftPx, result.image.size());
        result.screen     = screen;
        return result;
    }
};

std::unique_ptr<IScreenCapture> IScreenCapture::create() {
    return std::make_unique<QtScreenCapture>();
}

} // namespace igi::core
