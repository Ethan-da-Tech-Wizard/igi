#import <AppKit/AppKit.h>

// Suppresses the Dock icon at runtime when Igi is launched as a bare binary
// (without an app bundle that has LSUIElement=1 in its Info.plist).
// The definitive suppression for distribution is LSUIElement (Chunk 7).
extern "C" void igi_set_activation_policy_accessory() {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
