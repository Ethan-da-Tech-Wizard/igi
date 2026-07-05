#include <gtest/gtest.h>

#include <QGuiApplication>
#include <QScreen>

#include "core/ScreenGeometry.h"

using igi::core::ScreenGeometry;
using igi::core::ScreenInfo;

// These tests run against whatever displays the CI runner exposes.
// macos-14 GitHub runners present a single virtual display; we assert
// only the invariants that hold for any non-empty screen list.

TEST(ScreenGeometry, ReturnsValidPrimaryWhenAtPointOutsideAllScreens) {
    if (QGuiApplication::screens().isEmpty()) {
        GTEST_SKIP() << "no screens connected";
    }

    const QPoint farAway(-1'000'000, -1'000'000);
    const ScreenInfo info = ScreenGeometry::screenAt(farAway);

    EXPECT_GE(info.index, 0) << "must fall back to primary screen";
    EXPECT_GT(info.devicePixelRatio, 0.0);
    EXPECT_FALSE(info.virtualGeometry.isEmpty());
}

TEST(ScreenGeometry, ReturnsCorrectScreenForPointInsideIt) {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        GTEST_SKIP() << "no screens connected";
    }

    for (int i = 0; i < screens.size(); ++i) {
        const QPoint center = screens[i]->geometry().center();
        const ScreenInfo info = ScreenGeometry::screenAt(center);
        EXPECT_EQ(info.index, i);
        EXPECT_EQ(info.virtualGeometry, screens[i]->geometry());
        EXPECT_DOUBLE_EQ(info.devicePixelRatio, screens[i]->devicePixelRatio());
    }
}

TEST(ScreenGeometry, ScreenByIndexClampsOutOfRangeToPrimary) {
    if (QGuiApplication::screens().isEmpty()) {
        GTEST_SKIP() << "no screens connected";
    }

    const ScreenInfo info = ScreenGeometry::screenByIndex(99999);
    EXPECT_GE(info.index, 0);
    EXPECT_GT(info.devicePixelRatio, 0.0);
}

TEST(ScreenGeometry, PointToScreenPixelsScalesByDPR) {
    if (QGuiApplication::screens().isEmpty()) {
        GTEST_SKIP() << "no screens connected";
    }

    QScreen* primary = QGuiApplication::primaryScreen();
    const qreal dpr = primary->devicePixelRatio();
    const QPointF logical = primary->geometry().topLeft() + QPointF(10, 20);

    const QPointF px = ScreenGeometry::toScreenPixels(logical);
    EXPECT_DOUBLE_EQ(px.x(), logical.x() * dpr);
    EXPECT_DOUBLE_EQ(px.y(), logical.y() * dpr);
}

TEST(ScreenGeometry, RectToScreenPixelsScalesAllCorners) {
    if (QGuiApplication::screens().isEmpty()) {
        GTEST_SKIP() << "no screens connected";
    }

    QScreen* primary = QGuiApplication::primaryScreen();
    const qreal dpr = primary->devicePixelRatio();
    const QRectF logical(primary->geometry().topLeft() + QPoint(5, 7), QSize(40, 30));

    const QRectF px = ScreenGeometry::toScreenPixels(logical);
    EXPECT_DOUBLE_EQ(px.x(),       logical.x()      * dpr);
    EXPECT_DOUBLE_EQ(px.y(),       logical.y()      * dpr);
    EXPECT_DOUBLE_EQ(px.width(),   logical.width()  * dpr);
    EXPECT_DOUBLE_EQ(px.height(),  logical.height() * dpr);
}
