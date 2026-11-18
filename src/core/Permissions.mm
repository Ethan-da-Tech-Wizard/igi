// Program: igi - Permissions.mm
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: macOS implementation that checks Screen Recording and Accessibility permissions, requests them, and opens the matching Settings panes.

#include "core/Permissions.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace igi::core {

// Checks both permissions and triggers the system request when one is missing so the app appears in Settings.
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

// Opens the Screen Recording privacy pane.
void openScreenRecordingSettings() {
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:
            @"x-apple.systempreferences:com.apple.preference.security"
             "?Privacy_ScreenCapture"]];
}

// Opens the Accessibility privacy pane.
void openAccessibilitySettings() {
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:
            @"x-apple.systempreferences:com.apple.preference.security"
             "?Privacy_Accessibility"]];
}

}  // namespace igi::core
