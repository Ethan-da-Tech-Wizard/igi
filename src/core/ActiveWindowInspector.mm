#include "core/ActiveWindowInspector.h"

#include <QRect>
#include <QString>
#include <QUrl>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace igi::core {

namespace {

// AX message timeout: 250 ms. A foreign app that can't respond in this
// window probably can't be captured cleanly anyway; we'd rather fall
// through to "no active window" and skip the session.
constexpr float kAxTimeoutSeconds = 0.25f;

QString cfStringToQString(CFStringRef ref) {
    if (!ref) return {};
    NSString* s = (__bridge NSString*)ref;
    return QString::fromNSString(s);
}

bool copyAxString(AXUIElementRef element, CFStringRef attr, QString& out) {
    CFTypeRef value = nullptr;
    if (AXUIElementCopyAttributeValue(element, attr, &value) != kAXErrorSuccess
        || !value) {
        return false;
    }
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        out = cfStringToQString(static_cast<CFStringRef>(value));
        CFRelease(value);
        return true;
    }
    CFRelease(value);
    return false;
}

bool copyAxRect(AXUIElementRef element, QRect& out) {
    CFTypeRef posVal = nullptr;
    CFTypeRef sizeVal = nullptr;

    if (AXUIElementCopyAttributeValue(element, kAXPositionAttribute, &posVal)
            != kAXErrorSuccess || !posVal) {
        if (posVal) CFRelease(posVal);
        return false;
    }
    if (AXUIElementCopyAttributeValue(element, kAXSizeAttribute, &sizeVal)
            != kAXErrorSuccess || !sizeVal) {
        CFRelease(posVal);
        if (sizeVal) CFRelease(sizeVal);
        return false;
    }

    CGPoint pos{};
    CGSize  size{};
    AXValueGetValue(static_cast<AXValueRef>(posVal), static_cast<AXValueType>(kAXValueCGPointType), &pos);
    AXValueGetValue(static_cast<AXValueRef>(sizeVal), static_cast<AXValueType>(kAXValueCGSizeType), &size);
    CFRelease(posVal);
    CFRelease(sizeVal);

    out = QRect(static_cast<int>(pos.x),
                static_cast<int>(pos.y),
                static_cast<int>(size.width),
                static_cast<int>(size.height));
    return true;
}

}  // namespace

std::optional<ActiveWindowInfo> queryActiveWindow() {
    NSRunningApplication* frontApp =
        [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (!frontApp) {
        return std::nullopt;
    }

    AXUIElementRef appElement =
        AXUIElementCreateApplication(frontApp.processIdentifier);
    if (!appElement) {
        return std::nullopt;
    }

    AXUIElementSetMessagingTimeout(appElement, kAxTimeoutSeconds);

    CFTypeRef windowRef = nullptr;
    AXError err = AXUIElementCopyAttributeValue(
        appElement, kAXFocusedWindowAttribute, &windowRef);
    if (err != kAXErrorSuccess || !windowRef) {
        CFRelease(appElement);
        if (windowRef) CFRelease(windowRef);
        return std::nullopt;
    }

    AXUIElementRef window = static_cast<AXUIElementRef>(windowRef);
    AXUIElementSetMessagingTimeout(window, kAxTimeoutSeconds);

    ActiveWindowInfo info{};
    info.pid      = frontApp.processIdentifier;
    info.bundleId = QString::fromNSString(frontApp.bundleIdentifier ?: @"");
    info.appName  = QString::fromNSString(frontApp.localizedName ?: @"");

    if (!copyAxRect(window, info.bounds)) {
        CFRelease(window);
        CFRelease(appElement);
        return std::nullopt;
    }

    // kAXDocumentAttribute returns the document URL string when present
    // (Preview, TextEdit, etc.). Absent for browsers and apps that don't
    // expose the attribute — that's fine, Chunk 6 falls back to capture.
    QString documentString;
    if (copyAxString(window, kAXDocumentAttribute, documentString)) {
        // The attribute is typically a "file://..." URL; strip the prefix.
        if (documentString.startsWith(QStringLiteral("file://"))) {
            documentString = QUrl(documentString).toLocalFile();
        }
        info.documentPath = documentString;
    }

    CFRelease(window);
    CFRelease(appElement);
    return info;
}

}  // namespace igi::core
