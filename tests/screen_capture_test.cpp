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
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->image.isNull());
    EXPECT_GT(result->image.width(), 0);
    EXPECT_GT(result->image.height(), 0);
    EXPECT_EQ(result->image.format(), QImage::Format_ARGB32);
}
