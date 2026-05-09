#include "core/Permissions.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace igi::core {

PermissionStatus preflightPermissions() {
    PermissionStatus status;

    // Screen recording: CGPreflightScreenCaptureAccess() checks without prompting.
    // If it fails, we MUST call CGRequestScreenCaptureAccess() to force macOS
    // to add the app to the list in System Settings.
    status.screenRecording = CGPreflightScreenCaptureAccess();
    if (!status.screenRecording) {
        CGRequestScreenCaptureAccess();
    }

    // Accessibility: check without prompting first.
    NSDictionary* noPromptOpts = @{(__bridge id)kAXTrustedCheckOptionPrompt: @NO};
    status.accessibility = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)noPromptOpts);
    
    // If not granted, check again WITH prompting so it populates the Accessibility list.
    if (!status.accessibility) {
        NSDictionary* promptOpts = @{(__bridge id)kAXTrustedCheckOptionPrompt: @YES};
        AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)promptOpts);
    }

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
