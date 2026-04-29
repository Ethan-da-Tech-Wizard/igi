#pragma once

#include <QPointF>
#include <QRect>
#include <QRectF>

class QScreen;

namespace igi::core {

struct ScreenInfo {
    int      index;             // QGuiApplication::screens() index, -1 if none
    QRect    virtualGeometry;   // logical (point) coords in virtual desktop
    qreal    devicePixelRatio;
};

// DPR-aware, multi-monitor coordinate helpers. All inputs are in the
// macOS "virtual desktop" point space that QScreen exposes; outputs in
// physical pixels are needed by Leptonica/Tesseract and the Chunk 5
// highlight overlay.
class ScreenGeometry {
public:
    // Returns the screen containing the given virtual-space point. When
    // the point is outside every screen (e.g., disconnected display),
    // returns ScreenInfo for the primary screen. If no screens are
    // connected at all, returns {-1, {}, 1.0}.
    static ScreenInfo screenAt(QPoint virtualPoint);

    // Returns the screen with the given QGuiApplication index, or the
    // primary if out-of-range.
    static ScreenInfo screenByIndex(int index);

    // Convert a logical point (virtual desktop coords) to physical pixels
    // on the screen it lies on, scaling by that screen's DPR.
    static QPointF toScreenPixels(QPointF logicalPoint);

    // Convert a logical rect to physical pixels using the DPR of the
    // screen its top-left corner lies on. For rects spanning multiple
    // screens (rare), the top-left's DPR wins — caller should split if
    // exact handling matters.
    static QRectF toScreenPixels(QRectF logicalRect);

private:
    static ScreenInfo infoFor(QScreen* screen, int index);
};

}  // namespace igi::core
