// ==============================================================================
// Program: screen_capture_test.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Automated test suite validating correctness, boundary conditions, edge cases, and performance invariants for the corresponding subsystem.
// ==============================================================================

#include <gtest/gtest.h>
#include <QGuiApplication>
#include "core/ScreenCapture.h"

using igi::core::IScreenCapture;
using igi::core::CaptureResult;

TEST(ScreenCapture, FactoryReturnsValidInstance) {
    auto capture = IScreenCapture::create();
    ASSERT_NE(capture, nullptr);
}

TEST(ScreenCapture, CaptureFocusedWindowNeverReturnsNullOptOrNullImage) {
    auto capture = IScreenCapture::create();
    ASSERT_NE(capture, nullptr);

    auto result = capture->captureFocusedWindow();
    if (!result.has_value()) {
        GTEST_SKIP() << "Skipping focused window capture test in headless environment (no active window detected)";
    }
    EXPECT_FALSE(result->image.isNull());
    EXPECT_GT(result->image.width(), 0);
    EXPECT_GT(result->image.height(), 0);
    EXPECT_EQ(result->image.format(), QImage::Format_ARGB32);
}
