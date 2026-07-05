#pragma once

#include <memory>

struct Pix;

class QImage;

namespace igi::ocr {

// RAII deleter for Leptonica PIX. Per DECISIONS.md D-006, the deleter
// zeros the pixel buffer with explicit_bzero before pixDestroy so the
// optimiser cannot elide the wipe.
struct PixDeleter {
    void operator()(Pix* p) const noexcept;
};

using PixPtr = std::unique_ptr<Pix, PixDeleter>;

class ImageConverter {
public:
    // Convert a QImage into a Leptonica PIX (32 bpp, RGBA composite).
    // The PIX owns its own pixel buffer; the source QImage may be safely
    // destroyed after this call. Returns nullptr if the source image is
    // null or zero-sized.
    //
    // The conversion is correctness-first (per-pixel composition via
    // Leptonica's bit-position macros), not throughput-optimised. If
    // benchmarks under D-002 show this exceeds the 50 ms convert budget
    // for 4K captures, swap to a SIMD row converter.
    static PixPtr qImageToPix(const QImage& source);
};

}  // namespace igi::ocr
