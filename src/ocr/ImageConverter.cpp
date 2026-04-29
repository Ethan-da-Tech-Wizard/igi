#include "ocr/ImageConverter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <QImage>

#include <leptonica/allheaders.h>

#if defined(__APPLE__) || defined(__linux__)
#  include <strings.h>  // explicit_bzero
#endif

namespace igi::ocr {

namespace {

// Portable secure-wipe shim. macOS and recent glibc/musl provide
// explicit_bzero; if a future host lacks it, fall back to a volatile
// pointer overwrite.
void secure_zero(void* p, std::size_t n) noexcept {
#if defined(__APPLE__) || defined(__GLIBC__) || defined(__BIONIC__)
    explicit_bzero(p, n);
#else
    auto* v = static_cast<volatile unsigned char*>(p);
    while (n--) *v++ = 0;
#endif
}

}  // namespace

void PixDeleter::operator()(Pix* p) const noexcept {
    if (!p) return;

    // pixGetData returns the raw word buffer; pixGetWpl is words-per-line
    // accounting for Leptonica's internal alignment.
    if (l_uint32* data = pixGetData(p)) {
        const std::size_t bytes =
            static_cast<std::size_t>(pixGetWpl(p)) *
            static_cast<std::size_t>(pixGetHeight(p)) *
            sizeof(l_uint32);
        secure_zero(data, bytes);
    }

    pixDestroy(&p);
}

PixPtr ImageConverter::qImageToPix(const QImage& source) {
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return PixPtr{};
    }

    // Normalise to a known layout. ARGB32 gives us guaranteed 0xAARRGGBB
    // word values via QRgb regardless of the input format.
    const QImage rgb = (source.format() == QImage::Format_ARGB32)
        ? source
        : source.convertToFormat(QImage::Format_ARGB32);

    const int w = rgb.width();
    const int h = rgb.height();

    Pix* pix = pixCreate(w, h, 32);
    if (!pix) return PixPtr{};

    l_uint32*  dstData = pixGetData(pix);
    const int  dstWpl  = pixGetWpl(pix);

    for (int y = 0; y < h; ++y) {
        const auto* srcRow =
            reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        l_uint32* dstRow = dstData + static_cast<std::ptrdiff_t>(y) * dstWpl;

        for (int x = 0; x < w; ++x) {
            const QRgb c = srcRow[x];  // 0xAARRGGBB
            // Leptonica composite: (R<<24) | (G<<16) | (B<<8) | A,
            // interpreted as a uint32 word, regardless of host endianness.
            dstRow[x] = (static_cast<l_uint32>(qRed(c))   << 24)
                      | (static_cast<l_uint32>(qGreen(c)) << 16)
                      | (static_cast<l_uint32>(qBlue(c))  <<  8)
                      |  static_cast<l_uint32>(qAlpha(c));
        }
    }

    return PixPtr{pix};
}

}  // namespace igi::ocr
