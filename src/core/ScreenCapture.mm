#include "core/ScreenCapture.h"

#include "core/ActiveWindowInspector.h"
#include "core/ScreenGeometry.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <QImage>

namespace igi::core {

namespace {

// Find the CG window id whose owning PID + on-screen bounds best match
// the AX-reported active window. CGWindowListCreateImage takes a
// window id (or a screen rect), so we need to translate from AX-space
// to CG-space.
//
// On modern macOS, AX bounds and CG bounds are both in the global
// virtual desktop with the top-left of the primary display at (0,0),
// so they should agree directly. We still match by PID + intersection
// to be robust against off-by-one position differences.
CGWindowID findCgWindowFor(const ActiveWindowInfo& info) {
    CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!windows) return kCGNullWindowID;

    CGWindowID best = kCGNullWindowID;
    int        bestArea = -1;

    const CFIndex count = CFArrayGetCount(windows);
    for (CFIndex i = 0; i < count; ++i) {
        auto* dict = static_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(windows, i));

        CFNumberRef pidRef = static_cast<CFNumberRef>(
            CFDictionaryGetValue(dict, kCGWindowOwnerPID));
        if (!pidRef) continue;
        int32_t pid = 0;
        CFNumberGetValue(pidRef, kCFNumberSInt32Type, &pid);
        if (pid != info.pid) continue;

        CFDictionaryRef bounds = static_cast<CFDictionaryRef>(
            CFDictionaryGetValue(dict, kCGWindowBounds));
        if (!bounds) continue;

        CGRect rect{};
        CGRectMakeWithDictionaryRepresentation(bounds, &rect);
        QRect cgRect(static_cast<int>(rect.origin.x),
                     static_cast<int>(rect.origin.y),
                     static_cast<int>(rect.size.width),
                     static_cast<int>(rect.size.height));

        QRect overlap = cgRect.intersected(info.bounds);
        const int area = overlap.width() * overlap.height();
        if (area > bestArea) {
            CFNumberRef widRef = static_cast<CFNumberRef>(
                CFDictionaryGetValue(dict, kCGWindowNumber));
            CGWindowID wid = kCGNullWindowID;
            CFNumberGetValue(widRef, kCFNumberSInt64Type, &wid);
            best = wid;
            bestArea = area;
        }
    }

    CFRelease(windows);
    return best;
}

QImage cgImageToQImage(CGImageRef cgImage) {
    const size_t w = CGImageGetWidth(cgImage);
    const size_t h = CGImageGetHeight(cgImage);
    if (w == 0 || h == 0) return {};

    QImage out(static_cast<int>(w), static_cast<int>(h),
               QImage::Format_ARGB32);
    if (out.isNull()) return {};

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        out.bits(),
        w, h, 8, out.bytesPerLine(),
        cs,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);
    if (!ctx) return {};

    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), cgImage);
    CGContextRelease(ctx);
    return out;
}

class CGScreenCapture final : public IScreenCapture {
public:
    std::optional<CaptureResult> captureFocusedWindow() override {
        const auto active = queryActiveWindow();
        if (!active) return std::nullopt;

        const CGWindowID wid = findCgWindowFor(*active);
        if (wid == kCGNullWindowID) return std::nullopt;

        CGImageRef cg = CGWindowListCreateImage(
            CGRectNull,
            kCGWindowListOptionIncludingWindow,
            wid,
            kCGWindowImageBoundsIgnoreFraming | kCGWindowImageBestResolution);
        if (!cg) return std::nullopt;

        QImage image = cgImageToQImage(cg);
        CFRelease(cg);
        if (image.isNull()) return std::nullopt;

        const ScreenInfo screen = ScreenGeometry::screenAt(
            active->bounds.topLeft());

        // CGImage is already at physical pixel dimensions, so the
        // screenRect is its size. Origin is the AX bounds * DPR.
        const QPoint topLeftPx(
            static_cast<int>(active->bounds.x() * screen.devicePixelRatio),
            static_cast<int>(active->bounds.y() * screen.devicePixelRatio));

        CaptureResult result;
        result.image      = std::move(image);
        result.screenRect = QRect(topLeftPx, result.image.size());
        result.screen     = screen;
        return result;
    }
};

}  // namespace

std::unique_ptr<IScreenCapture> IScreenCapture::create() {
    return std::make_unique<CGScreenCapture>();
}

}  // namespace igi::core
