// Program: igi - ScreenGeometry.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Implements screen lookup by point or index and logical-to-physical pixel conversion using Qt screens.

#include "core/ScreenGeometry.h"

#include <QGuiApplication>
#include <QScreen>

namespace igi::core {

// Builds a ScreenInfo from a QScreen, or a placeholder when the screen is null.
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

// Finds the screen containing the point, falling back to the primary screen.
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

// Looks up a screen by index, falling back to the primary screen when out of range.
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

// Scales a logical point by the device pixel ratio of its screen.
QPointF ScreenGeometry::toScreenPixels(QPointF logicalPoint) {
    const ScreenInfo info = screenAt(logicalPoint.toPoint());
    return logicalPoint * info.devicePixelRatio;
}

// Scales a logical rectangle using the device pixel ratio of its top-left corner's screen.
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
