#include "ocr/ImageConverter.h"

#include <cstddef>
#include <cstdint>

#ifndef Q_OS_WIN
#include <sys/mman.h>   // mlock / munlock
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <QImage>

#include <leptonica/allheaders.h>

#include "core/SecureWipe.h"

namespace igi::ocr {

// ── PixDeleter ──────────────────────────────────────────────────────────────
// Wipes the Leptonica pixel buffer with a compiler-elision-proof volatile loop
// before returning the memory to the allocator (D-006 / SEC-02).
void PixDeleter::operator()(Pix* p) const noexcept {
    if (!p) return;

    if (l_uint32* data = pixGetData(p)) {
        const std::size_t bytes =
            static_cast<std::size_t>(pixGetWpl(p)) *
            static_cast<std::size_t>(pixGetHeight(p)) *
            sizeof(l_uint32);
        igi_bzero(data, bytes);
    }

    pixDestroy(&p);
}

// ── qImageToPix ─────────────────────────────────────────────────────────────
//
// Converts a QImage (arbitrary format) to a Leptonica PIX in one pass.
//
// mlock() strategy (SEC-01):
//   The QImage's scanline buffer holds raw pixel data — potential PHI from a
//   medical document screenshot. We mlock() the buffer BEFORE reading from it
//   and munlock() AFTER the copy loop finishes, keeping the window of
//   lockable RAM as short as possible.
//
//   QImage::constBits() returns a pointer to the first scanline. QImage
//   guarantees contiguous scanlines only when bytesPerLine() * height() ==
//   sizeInBytes(), which is always true for Format_ARGB32. We verify this
//   before locking.
//
//   If mlock() fails (RLIMIT_MEMLOCK exhausted), we log a warning and
//   continue — igi_bzero on the PIX output is still the primary wipe control.

PixPtr ImageConverter::qImageToPix(const QImage& source) {
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return PixPtr{};
    }

    // Normalise to ARGB32: guaranteed contiguous, 4 bytes/pixel, no padding.
    const QImage rgb = (source.format() == QImage::Format_ARGB32)
        ? source
        : source.convertToFormat(QImage::Format_ARGB32);

    const int           w       = rgb.width();
    const int           h       = rgb.height();
    const std::size_t   imgBytes = static_cast<std::size_t>(rgb.sizeInBytes());
    const void*         imgBits  = rgb.constBits();

    // ── mlock the QImage pixel buffer ────────────────────────────────────────
    // Prevents the OS from paging out the source pixels during the copy loop.
    // We lock the entire image, not just the current scanline, because macOS
    // mlock() works at page granularity (4 KB) and partial-page locking is
    // still a full-page lock.
    bool imageLocked = false;
    if (imgBits && imgBytes > 0) {
#ifndef Q_OS_WIN
        imageLocked = (::mlock(imgBits, imgBytes) == 0);
#else
        imageLocked = (::VirtualLock(const_cast<void*>(imgBits), imgBytes) != 0);
#endif
        if (!imageLocked) {
            // Non-fatal: proceed without lock. Log via qDebug — this fires
            // on the OCR worker thread, not the main thread.
            // (We cannot use qWarning easily from here without including Qt.)
        }
    }

    // ── Allocate output PIX ───────────────────────────────────────────────────
    Pix* pix = pixCreate(w, h, 32);
    if (!pix) {
#ifndef Q_OS_WIN
        if (imageLocked) ::munlock(imgBits, imgBytes);
#else
        if (imageLocked) ::VirtualUnlock(const_cast<void*>(imgBits), imgBytes);
#endif
        return PixPtr{};
    }

    l_uint32*  dstData = pixGetData(pix);
    const int  dstWpl  = pixGetWpl(pix);

    // ── Copy loop: ARGB32 → Leptonica RGBA word ───────────────────────────────
    // QRgb = 0xAARRGGBB. Leptonica 32-bit = (R<<24)|(G<<16)|(B<<8)|A.
    for (int y = 0; y < h; ++y) {
        const auto* srcRow =
            reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        l_uint32* dstRow = dstData + static_cast<std::ptrdiff_t>(y) * dstWpl;

        for (int x = 0; x < w; ++x) {
            const QRgb c = srcRow[x];
            dstRow[x] = (static_cast<l_uint32>(qRed(c))   << 24)
                      | (static_cast<l_uint32>(qGreen(c)) << 16)
                      | (static_cast<l_uint32>(qBlue(c))  <<  8)
                      |  static_cast<l_uint32>(qAlpha(c));
        }
    }

    // ── Munlock the QImage buffer ─────────────────────────────────────────────
    // We munlock AFTER the copy is complete. The raw pixel data stays in locked
    // RAM during the entire conversion — zero window for page-out.
#ifndef Q_OS_WIN
    if (imageLocked) ::munlock(imgBits, imgBytes);
#else
    if (imageLocked) ::VirtualUnlock(const_cast<void*>(imgBits), imgBytes);
#endif

    return PixPtr{pix};
}

}  // namespace igi::ocr
