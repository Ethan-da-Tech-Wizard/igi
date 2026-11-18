// Program: igi - screen_capture_test.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Tests that the screen capture factory works and that a capture always yields a usable image.

#include <gtest/gtest.h>
#include <QGuiApplication>
#include "core/ScreenCapture.h"

using igi::core::IScreenCapture;
using igi::core::CaptureResult;

// The factory returns an object.
TEST(ScreenCapture, FactoryReturnsValidInstance) {
    auto capture = IScreenCapture::create();
    ASSERT_NE(capture, nullptr);
}

// A capture always returns a non-empty ARGB32 image.
TEST(ScreenCapture, CaptureFocusedWindowNeverReturnsNullOptOrNullImage) {
    auto capture = IScreenCapture::create();
    ASSERT_NE(capture, nullptr);

    auto result = capture->captureFocusedWindow();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->image.isNull());
    EXPECT_GT(result->image.width(), 0);
    EXPECT_GT(result->image.height(), 0);
    EXPECT_EQ(result->image.format(), QImage::Format_ARGB32);
}
