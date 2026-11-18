// ==============================================================================
// Program: AppPolicy.mm
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Configures OS application presentation modes, such as macOS NSApplicationActivationPolicyAccessory, to allow background daemon operation without docking or menu bar clutter.
// ==============================================================================

#import <AppKit/AppKit.h>

// Suppresses the Dock icon at runtime when Igi is launched as a bare binary
// (without an app bundle that has LSUIElement=1 in its Info.plist).
// The definitive suppression for distribution is LSUIElement (Chunk 7).
extern "C" void igi_set_activation_policy_accessory() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
