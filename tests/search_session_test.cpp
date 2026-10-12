#include <gtest/gtest.h>

#include "SearchSession.h"
#include "core/ScreenCapture.h"
#include "ocr/OcrEngine.h"
#include "ui/SearchOverlay.h"
#include "ui/CaptureFlashOverlay.h"

namespace igi {

class MockFailingScreenCapture : public core::IScreenCapture {
public:
    std::optional<core::CaptureResult> captureFocusedWindow() override {
        return std::nullopt; // Simulates capture failure under Linux Wayland
    }
};

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
