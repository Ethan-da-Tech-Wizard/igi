// Program: igi - search_session_test.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Tests that SearchSession still shows its overlays when screen capture fails or returns a null image.

#include <gtest/gtest.h>

#include "SearchSession.h"
#include "core/ScreenCapture.h"
#include "ocr/OcrEngine.h"
#include "ui/SearchOverlay.h"
#include "ui/CaptureFlashOverlay.h"

namespace igi {

// Mock capture that always fails.
class MockFailingScreenCapture : public core::IScreenCapture {
public:
    std::optional<core::CaptureResult> captureFocusedWindow() override {
        return std::nullopt; // Simulates capture failure under Linux Wayland
    }
};

// Mock capture that returns a result with a null image.
class MockNullImageScreenCapture : public core::IScreenCapture {
public:
    std::optional<core::CaptureResult> captureFocusedWindow() override {
        core::CaptureResult res;
        res.screenRect = QRect(0, 0, 1920, 1080);
        res.screen = {0, QRect(0, 0, 1920, 1080), 1.0};
        res.image = QImage(); // Null image
        return res;
    }
};

// When capture fails, activation still shows the overlay.
TEST(SearchSessionTest, ActivateSucceedsAndShowsOverlaysWhenCaptureReturnsNullopt) {
    MockFailingScreenCapture capture;
    ocr::OcrEngine ocrEngine;
    ocrEngine.init(nullptr, "eng");

    ui::SearchOverlay overlay;
    ui::CaptureFlashOverlay flashOverlay;

    SearchSession session(&capture, &ocrEngine, &overlay, &flashOverlay);

    session.activate();

    EXPECT_TRUE(overlay.isVisible());

    // Check search overlay bottom-center positioning
    const QRect overlayGeo = overlay.geometry();
    EXPECT_GT(overlayGeo.width(), 0);
    EXPECT_GT(overlayGeo.height(), 0);

    session.dismiss();
    EXPECT_FALSE(overlay.isVisible());
}

// When the image is null, activation still shows the overlay.
TEST(SearchSessionTest, ActivateSucceedsAndShowsOverlaysWhenCaptureImageIsNull) {
    MockNullImageScreenCapture capture;
    ocr::OcrEngine ocrEngine;
    ocrEngine.init(nullptr, "eng");

    ui::SearchOverlay overlay;
    ui::CaptureFlashOverlay flashOverlay;

    SearchSession session(&capture, &ocrEngine, &overlay, &flashOverlay);

    session.activate();

    EXPECT_TRUE(overlay.isVisible());

    session.dismiss();
    EXPECT_FALSE(overlay.isVisible());
}

} // namespace igi
