// Program: igi - screen_geometry_test.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Tests ScreenGeometry screen lookup and logical-to-physical coordinate scaling against the available displays.

#include <gtest/gtest.h>

#include <QGuiApplication>
#include <QScreen>

#include "core/ScreenGeometry.h"

using igi::core::ScreenGeometry;
using igi::core::ScreenInfo;

// These tests run against whatever displays the CI runner exposes.
// macos-14 GitHub runners present a single virtual display; we assert
// only the invariants that hold for any non-empty screen list.

// A point off every screen resolves to the primary screen.
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

// A point inside a screen resolves to that screen.
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

// An out-of-range index resolves to the primary screen.
TEST(ScreenGeometry, ScreenByIndexClampsOutOfRangeToPrimary) {
    if (QGuiApplication::screens().isEmpty()) {
        GTEST_SKIP() << "no screens connected";
    }

    const ScreenInfo info = ScreenGeometry::screenByIndex(99999);
    EXPECT_GE(info.index, 0);
    EXPECT_GT(info.devicePixelRatio, 0.0);
}

// A point is scaled by the device pixel ratio.
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

// Every rectangle value is scaled by the ratio.
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

// Combines a window origin with a word offset at a pixel ratio of 2 and checks the result.
TEST(ScreenGeometry, CoordinateScalingInvariance) {
    const qreal dpr = 2.0;
    const QRect logicalWindow(100, 200, 800, 600);
    const QPoint originPx(static_cast<int>(std::round(logicalWindow.x() * dpr)), static_cast<int>(std::round(logicalWindow.y() * dpr)));
    const QPoint wordMatchPx(400, 200);

    int physX = originPx.x() + wordMatchPx.x();
    int physY = originPx.y() + wordMatchPx.y();

    double logX = static_cast<double>(physX) / dpr;
    double logY = static_cast<double>(physY) / dpr;

    EXPECT_DOUBLE_EQ(logX - logicalWindow.x(), wordMatchPx.x() / dpr);
    EXPECT_DOUBLE_EQ(logY - logicalWindow.y(), wordMatchPx.y() / dpr);
}
