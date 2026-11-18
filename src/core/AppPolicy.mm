// Program: igi - AppPolicy.mm
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: macOS helper that switches the app to accessory mode so it shows no Dock icon.

#import <AppKit/AppKit.h>

// Suppresses the Dock icon at runtime when Igi is launched as a bare binary
// (without an app bundle that has LSUIElement=1 in its Info.plist).
// The definitive suppression for distribution is LSUIElement (Chunk 7).
// Exported with C linkage so main.cpp can call it without Objective-C.
extern "C" void igi_set_activation_policy_accessory() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
