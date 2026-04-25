#include "core/Permissions.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace igi::core {

PermissionStatus preflightPermissions() {
    PermissionStatus status;

    // Screen recording: CGPreflightScreenCaptureAccess() checks without
    // triggering a TCC prompt. On macOS 10.15+ only.
    status.screenRecording = CGPreflightScreenCaptureAccess();

    // Accessibility: pass kAXTrustedCheckOptionPrompt = false to avoid
    // auto-prompting — we manage that ourselves with a friendlier dialog.
    NSDictionary* opts = @{(__bridge id)kAXTrustedCheckOptionPrompt: @NO};
    status.accessibility = AXIsProcessTrustedWithOptions(
        (__bridge CFDictionaryRef)opts);

    return status;
}

void openScreenRecordingSettings() {
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:
            @"x-apple.systempreferences:com.apple.preference.security"
             "?Privacy_ScreenCapture"]];
}

void openAccessibilitySettings() {
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:
            @"x-apple.systempreferences:com.apple.preference.security"
             "?Privacy_Accessibility"]];
}

}  // namespace igi::core
