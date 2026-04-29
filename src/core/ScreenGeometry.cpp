#include "core/ScreenGeometry.h"

#include <QGuiApplication>
#include <QScreen>

namespace igi::core {

ScreenInfo ScreenGeometry::infoFor(QScreen* screen, int index) {
    if (!screen) {
        return {-1, {}, 1.0};
    }
    return {
        index,
        screen->geometry(),
        screen->devicePixelRatio(),
    };
}

ScreenInfo ScreenGeometry::screenAt(QPoint virtualPoint) {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return {-1, {}, 1.0};
    }

    for (int i = 0; i < screens.size(); ++i) {
        if (screens[i]->geometry().contains(virtualPoint)) {
            return infoFor(screens[i], i);
        }
    }

    // Fallback: primary screen.
    QScreen* primary = QGuiApplication::primaryScreen();
    const int idx = screens.indexOf(primary);
    return infoFor(primary, idx);
}

ScreenInfo ScreenGeometry::screenByIndex(int index) {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return {-1, {}, 1.0};
    }
    if (index < 0 || index >= screens.size()) {
        QScreen* primary = QGuiApplication::primaryScreen();
        return infoFor(primary, screens.indexOf(primary));
    }
    return infoFor(screens[index], index);
}

QPointF ScreenGeometry::toScreenPixels(QPointF logicalPoint) {
    const ScreenInfo info = screenAt(logicalPoint.toPoint());
    return logicalPoint * info.devicePixelRatio;
}

QRectF ScreenGeometry::toScreenPixels(QRectF logicalRect) {
    const ScreenInfo info = screenAt(logicalRect.topLeft().toPoint());
    const qreal dpr = info.devicePixelRatio;
    return QRectF{
        logicalRect.x()      * dpr,
        logicalRect.y()      * dpr,
        logicalRect.width()  * dpr,
        logicalRect.height() * dpr,
    };
}

}  // namespace igi::core
