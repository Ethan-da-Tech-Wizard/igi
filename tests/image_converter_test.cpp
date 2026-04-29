#include <gtest/gtest.h>

#include <QImage>

#include <leptonica/allheaders.h>

#include "ocr/ImageConverter.h"

using igi::ocr::ImageConverter;
using igi::ocr::PixPtr;

namespace {

QImage makeFixture(int w, int h) {
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Distinctive per-pixel values so any byte-order bug shows up.
            const int r = (x * 7 + 11) & 0xFF;
            const int g = (y * 13 + 17) & 0xFF;
            const int b = ((x + y) * 5 + 23) & 0xFF;
            img.setPixel(x, y, qRgba(r, g, b, 255));
        }
    }
    return img;
}

}  // namespace

TEST(ImageConverter, NullSourceReturnsNullPix) {
    EXPECT_EQ(ImageConverter::qImageToPix(QImage{}).get(), nullptr);
}

TEST(ImageConverter, RoundTripsRGBPixelsExactly) {
    const QImage src = makeFixture(7, 5);  // odd sizes catch stride bugs

    PixPtr pix = ImageConverter::qImageToPix(src);
    ASSERT_NE(pix.get(), nullptr);

    EXPECT_EQ(pixGetWidth(pix.get()),  src.width());
    EXPECT_EQ(pixGetHeight(pix.get()), src.height());
    EXPECT_EQ(pixGetDepth(pix.get()),  32);

    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            l_uint32 word = 0;
            ASSERT_EQ(pixGetPixel(pix.get(), x, y, &word), 0);

            l_int32 r = 0, g = 0, b = 0;
            extractRGBValues(word, &r, &g, &b);

            const QRgb expected = src.pixel(x, y);
            EXPECT_EQ(r, qRed(expected))   << "at (" << x << "," << y << ")";
            EXPECT_EQ(g, qGreen(expected)) << "at (" << x << "," << y << ")";
            EXPECT_EQ(b, qBlue(expected))  << "at (" << x << "," << y << ")";
        }
    }
}

TEST(ImageConverter, AcceptsNonARGB32Format) {
    QImage src(4, 4, QImage::Format_RGB32);
    src.fill(qRgb(50, 100, 200));

    PixPtr pix = ImageConverter::qImageToPix(src);
    ASSERT_NE(pix.get(), nullptr);

    l_uint32 word = 0;
    ASSERT_EQ(pixGetPixel(pix.get(), 0, 0, &word), 0);
    l_int32 r = 0, g = 0, b = 0;
    extractRGBValues(word, &r, &g, &b);

    EXPECT_EQ(r, 50);
    EXPECT_EQ(g, 100);
    EXPECT_EQ(b, 200);
}

TEST(ImageConverter, DeleterRunsAndDoesNotCrash) {
    // PixDeleter zeros the buffer before pixDestroy. We can't observe the
    // wipe (the buffer is freed), so this test just exercises the path
    // for ASan/UBSan to catch any UAF or double-free.
    {
        PixPtr p = ImageConverter::qImageToPix(makeFixture(16, 8));
        ASSERT_NE(p.get(), nullptr);
    }  // p goes out of scope -> deleter runs
    SUCCEED();
}
