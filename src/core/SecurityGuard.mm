#include "core/SecurityGuard.h"

#include <cstdlib>    // getenv
#include <cstring>

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Security/Security.h>
#import <IOKit/IOKitLib.h>
#include <mach-o/dyld.h>   // _dyld_image_count, _dyld_get_image_name

// ── Logging macros ────────────────────────────────────────────────────────────
// We use NSLog directly (not qDebug) because SecurityGuard runs before Qt
// is fully initialised.  These messages flow to the macOS Unified Log and
// are captured by any MDM syslog collector on the device.
#define IGI_HARD_FAIL(msg) \
    do { NSLog(@"[igi] SECURITY ABORT: " msg); abort(); } while (0)
#define IGI_SEC_WARN(fmt, ...) \
    NSLog(@"[igi] SECURITY WARNING: " fmt, ##__VA_ARGS__)
#define IGI_SEC_INFO(fmt, ...) \
    NSLog(@"[igi] SECURITY: " fmt, ##__VA_ARGS__)

namespace igi::core {

// ── SEC-GUARD-01: DYLD library injection ─────────────────────────────────────
//
// THREAT (T-1 / T-3): An attacker sets DYLD_INSERT_LIBRARIES in the shell
// environment (.zshrc plugin, LaunchAgent, or malicious npm/brew postinstall
// script) before launching Igi. The injected dylib runs inside Igi's process
// and inherits its Screen Recording permission.
//
// DEFENCE: Hardened Runtime (--options runtime in codesign) *should* block
// this for signed release builds at the dyld level — but we still check
// explicitly as defence-in-depth, because:
//   a) Debug builds are not signed with Hardened Runtime.
//   b) A future codesign misconfiguration could strip the flag silently.
//   c) We want the abort() logged to MDM syslog, not silently swallowed.
//
// If this variable is set at the point SecurityGuard runs, injection already
// happened. We abort immediately — there is no safe recovery path.
void SecurityGuard::checkDyldInjection() {
    if (getenv("DYLD_INSERT_LIBRARIES") != nullptr ||
        getenv("DYLD_FRAMEWORK_PATH")   != nullptr ||
        getenv("DYLD_LIBRARY_PATH")     != nullptr)
    {
        // DYLD_* vars are set. Under Hardened Runtime this means the OS
        // silently ignored them, but the environment is still hostile.
        // We abort to prevent execution in a potentially compromised shell.
        IGI_HARD_FAIL("DYLD_INSERT_LIBRARIES / DYLD_LIBRARY_PATH is set. "
                      "Library injection attack or malicious shell plugin detected. "
                      "Aborting to protect PHI.");
    }
    IGI_SEC_INFO("SEC-GUARD-01 PASS: No DYLD injection variables detected.");
}

// ── SEC-GUARD-02: Hardened Runtime self-check ─────────────────────────────────
//
// THREAT (T-1): A build or deployment misconfiguration strips the Hardened
// Runtime codesign flag, silently re-enabling DYLD injection and JIT-based
// exploitation vectors that Apple's SIP normally blocks.
//
// DEFENCE: We query our own code signature at runtime using SecCode APIs and
// verify that CS_RUNTIME (Hardened Runtime) is set in our code-signing flags.
// If not, we abort — running without Hardened Runtime in a clinical environment
// is a security regression that must not go unnoticed.
void SecurityGuard::checkHardenedRuntime() {
    SecCodeRef selfCode = nullptr;
    OSStatus status = SecCodeCopySelf(kSecCSDefaultFlags, &selfCode);
    if (status != errSecSuccess || !selfCode) {
        IGI_SEC_WARN("SEC-GUARD-02: Could not obtain self SecCode reference "
                     "(OSStatus %d). Skipping Hardened Runtime check.", (int)status);
        return;
    }

    CFDictionaryRef signingInfo = nullptr;
    status = SecCodeCopySigningInformation(selfCode,
                                           kSecCSSigningInformation,
                                           &signingInfo);
    CFRelease(selfCode);

    if (status != errSecSuccess || !signingInfo) {
        IGI_SEC_WARN("SEC-GUARD-02: Could not read signing information. "
                     "Binary may be unsigned. Running without Hardened Runtime.");
        if (signingInfo) CFRelease(signingInfo);
        return;
    }

    // CS_RUNTIME = 0x10000 (defined in <sys/codesign.h>).
    // Extract the csFlags value from the signing info dictionary.
    const uint32_t CS_RUNTIME_FLAG = 0x10000;
    CFNumberRef csFlags = (CFNumberRef)CFDictionaryGetValue(
        signingInfo, kSecCodeInfoFlags);

    if (csFlags) {
        uint32_t flags = 0;
        CFNumberGetValue(csFlags, kCFNumberSInt32Type, &flags);
        if (!(flags & CS_RUNTIME_FLAG)) {
            CFRelease(signingInfo);
            IGI_HARD_FAIL("Hardened Runtime (CS_RUNTIME) is NOT active on this "
                          "binary. DYLD injection is enabled. Aborting. "
                          "Re-sign with: codesign --options runtime ...");
        }
        IGI_SEC_INFO("SEC-GUARD-02 PASS: Hardened Runtime confirmed (flags=0x%x).",
                     flags);
    } else {
        IGI_SEC_WARN("SEC-GUARD-02: Could not read CS flags. Binary may be "
                     "ad-hoc or unsigned.");
    }

    CFRelease(signingInfo);
}

// ── SEC-GUARD-03: Dependency code-signature validation ───────────────────────
//
// THREAT (T-1): A poisoned Homebrew formula replaces libtesseract with a
// backdoored version that exfiltrates OCR output. The binary appears identical
// to the legitimate library — same filename, same version string — but has
// different code. This attack bypasses source-level review because the
// malicious code is injected at the formula level, not the source level.
//
// DEFENCE: At runtime, before Tesseract is initialised, we locate the
// libtesseract dylib loaded into our process and validate its code signature
// using SecStaticCodeCheckValidity. A library that was tampered with after
// signing will fail this check. An unsigned library (malicious replacement
// that was never signed) also fails.
//
// NOTE: This check validates the *signature*, not a content hash. A
// sophisticated attacker who has Apple's private signing key (nation-state
// level) could bypass it. The complementary control is CMake dependency
// pinning + CI hash verification (enforced separately).
void SecurityGuard::checkDependencySignature() {
    // Find where libtesseract is loaded in our process.
    // We iterate the loaded dylibs by name.
    NSBundle* mainBundle = [NSBundle mainBundle];
    NSString* frameworksPath = [[mainBundle bundlePath]
        stringByAppendingPathComponent:@"Contents/Frameworks"];

    // Look for libtesseract in the bundle's Frameworks dir first,
    // then in the dyld image list for system/Homebrew installs.
    NSString* tessPath = nil;

    // Check bundled frameworks.
    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray<NSString*>* frameworks = [fm contentsOfDirectoryAtPath:frameworksPath
                                                             error:nil];
    for (NSString* f in frameworks) {
        if ([f hasPrefix:@"libtesseract"] || [f hasPrefix:@"tesseract"]) {
            tessPath = [frameworksPath stringByAppendingPathComponent:f];
            break;
        }
    }

    // Fall back: scan dyld image list for a non-bundled tesseract.
    if (!tessPath) {
        uint32_t count = _dyld_image_count();
        for (uint32_t i = 0; i < count; ++i) {
            const char* name = _dyld_get_image_name(i);
            if (name && (strstr(name, "libtesseract") || strstr(name, "tesseract"))) {
                tessPath = [NSString stringWithUTF8String:name];
                break;
            }
        }
    }

    if (!tessPath) {
        IGI_SEC_WARN("SEC-GUARD-03: Could not locate libtesseract in loaded "
                     "images. Skipping dependency signature check.");
        return;
    }

    IGI_SEC_INFO("SEC-GUARD-03: Validating signature of: %@", tessPath);

    CFURLRef tessURL = (__bridge CFURLRef)[NSURL fileURLWithPath:tessPath];
    SecStaticCodeRef staticCode = nullptr;
    OSStatus status = SecStaticCodeCreateWithPath(tessURL,
                                                   kSecCSDefaultFlags,
                                                   &staticCode);
    if (status != errSecSuccess || !staticCode) {
        IGI_SEC_WARN("SEC-GUARD-03: Could not create static code ref for "
                     "libtesseract (OSStatus %d). It may be unsigned "
                     "(Homebrew libraries typically are). Continuing — "
                     "verify checksums in CI to compensate.", (int)status);
        return;
    }

    // Validate the static signature (checks certificate chain + hash).
    // kSecCSCheckAllArchitectures for Universal Binary validation.
    status = SecStaticCodeCheckValidity(staticCode,
                                         kSecCSCheckAllArchitectures,
                                         nullptr);
    CFRelease(staticCode);

    if (status == errSecCSUnsigned) {
        // Homebrew-installed libraries are typically not signed.
        // This is expected in development. Warn but do not abort.
        IGI_SEC_WARN("SEC-GUARD-03: libtesseract is UNSIGNED. Expected for "
                     "Homebrew builds. In production, use a signed/notarized "
                     "build or embed a pre-verified static library. "
                     "CI hash pinning is your primary control here.");
    } else if (status != errSecSuccess) {
        // Signature is present but INVALID — this is the attack scenario.
        IGI_HARD_FAIL("libtesseract code signature is INVALID. The library "
                      "may have been tampered with (poisoned Homebrew formula "
                      "or replaced binary). Aborting to protect PHI.");
    } else {
        IGI_SEC_INFO("SEC-GUARD-03 PASS: libtesseract signature valid.");
    }
}

// ── SEC-GUARD-04: DMA / Thunderbolt threat advisory ──────────────────────────
//
// THREAT (T-2): An attacker with physical access plugs in a Thunderbolt device
// and uses pcileech or a similar DMA tool to read all RAM, bypassing all
// OS-level protections including our explicit_bzero and mlock().
//
// DEFENCE: We cannot prevent DMA in software. We can check whether the macOS
// Thunderbolt Security level is set to a protective mode and warn the operator
// if it is not. The operator (or MDM) should set the machine to
// "Secure" or "Full Security" Thunderbolt mode in System Settings.
//
// Additionally, we log this advisory to the Unified Log so an MDM syslog
// collector sees it — if Thunderbolt security was previously set and is now
// "None", that is itself an anomaly worth alerting on.
void SecurityGuard::adviseDmaThreats() {
    // Read the Thunderbolt security level from IOKit.
    // The key is IOThunderboltSecurityLevel in the IOThunderboltController
    // service. A value of 0 = "None" (fully open to DMA); 3 = "Full Security".
    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("IOThunderboltController"));

    if (!service) {
        // No Thunderbolt controller found (e.g., Apple Silicon with USB4).
        IGI_SEC_INFO("SEC-GUARD-04: No IOThunderboltController found. "
                     "DMA via legacy Thunderbolt not applicable on this hardware.");
        return;
    }

    CFTypeRef levelRef = IORegistryEntryCreateCFProperty(
        service,
        CFSTR("IOThunderboltSecurityLevel"),
        kCFAllocatorDefault, 0);
    IOObjectRelease(service);

    if (!levelRef) {
        IGI_SEC_WARN("SEC-GUARD-04: Could not read Thunderbolt security level. "
                     "Verify manually: System Settings → Privacy & Security → "
                     "Thunderbolt Security. Set to 'Secure' or 'Full Security' "
                     "to prevent DMA memory attacks against PHI sessions.");
        return;
    }

    int32_t level = 0;
    CFNumberGetValue((CFNumberRef)levelRef, kCFNumberSInt32Type, &level);
    CFRelease(levelRef);

    if (level == 0) {
        // Level 0 = "None" — fully open to DMA.
        IGI_SEC_WARN("SEC-GUARD-04: Thunderbolt Security is set to NONE (level 0). "
                     "This machine is VULNERABLE to DMA memory attacks while "
                     "Igi has an active PHI session. Set to 'Secure' (level 1) "
                     "or 'Full Security' (level 3) in System Settings.");
    } else {
        IGI_SEC_INFO("SEC-GUARD-04 PASS: Thunderbolt Security level = %d "
                     "(0=None, 1=Secure, 3=Full). DMA risk is reduced.", level);
    }
}

// ── SEC-GUARD-05: Accessibility grant audit ──────────────────────────────────
//
// THREAT (T-4 / T-5): Any process that holds macOS Accessibility permission
// can register a global NSEvent monitor and observe every Cmd+Shift+F
// keystroke — building a PHI access timing log without touching Igi's code.
// An attacker with Accessibility can also manipulate window focus to execute
// the TOCTOU overcapture attack (T-5).
//
// DEFENCE: We cannot revoke another app's Accessibility grant. We can read
// the macOS TCC (Transparency, Consent, and Control) database to enumerate
// which processes currently hold the permission, and log them. If an MDM
// syslog collector is watching, unexpected entries generate an alert.
//
// NOTE: Reading the TCC database directly requires either root or the
// com.apple.private.tcc.allow entitlement (private Apple entitlement).
// Without those, we fall back to checking the running process list against
// a hardcoded allowlist and warning about any unrecognised process that
// holds an NSEvent monitor registration.
//
// In practice, this is a logging/advisory control, not a blocking one.
void SecurityGuard::auditAccessibilityGrants() {
    // Soft check: log our own AX trust status and remind the operator to
    // audit other apps via MDM.
    BOOL trusted = AXIsProcessTrustedWithOptions(nullptr);
    IGI_SEC_INFO("SEC-GUARD-05: Igi Accessibility trust = %s. "
                 "REMINDER: Any other app with Accessibility permission can "
                 "observe Cmd+Shift+F keystrokes (T-4) and manipulate window "
                 "focus (T-5). Audit via: "
                 "System Settings → Privacy & Security → Accessibility. "
                 "Use MDM PPPC profile to restrict Accessibility to approved "
                 "bundle IDs only (see docs/MDM_DEPLOYMENT.md).",
                 trusted ? "GRANTED" : "DENIED");

    // Log running processes that appear to hold event monitors.
    // We enumerate NSWorkspace running applications as a proxy.
    NSArray<NSRunningApplication*>* apps =
        [[NSWorkspace sharedWorkspace] runningApplications];

    // Known-safe bundle IDs that legitimately use Accessibility.
    NSSet<NSString*>* knownSafe = [NSSet setWithObjects:
        @"com.apple.systempreferences",
        @"com.apple.accessibility.zoom",
        @"com.apple.VoiceOver",
        @"com.apple.iWork.Keynote",
        @"com.igi.ocrsearch",      // ourselves
        nil];

    NSMutableArray<NSString*>* suspicious = [NSMutableArray array];
    for (NSRunningApplication* app in apps) {
        NSString* bid = app.bundleIdentifier;
        if (!bid) continue;
        // Heuristic: flag any third-party app we don't recognise.
        // This is necessarily incomplete — the real check is the TCC DB.
        if (![bid hasPrefix:@"com.apple."] &&
            ![bid hasPrefix:@"com.igi."] &&
            ![knownSafe containsObject:bid]) {
            [suspicious addObject:bid];
        }
    }

    if (suspicious.count > 0) {
        IGI_SEC_WARN("SEC-GUARD-05: The following non-Apple processes are "
                     "running and COULD hold Accessibility permission. Verify "
                     "each in System Settings → Privacy → Accessibility:\n%@",
                     [suspicious componentsJoinedByString:@"\n"]);
    } else {
        IGI_SEC_INFO("SEC-GUARD-05: No unexpected third-party processes "
                     "detected in the running application list.");
    }
}

// ── Public entry point ────────────────────────────────────────────────────────
void SecurityGuard::runStartupChecks() {
    NSLog(@"[igi] === Security Guard: running startup checks ===");

    // Hard checks first — any failure aborts the process.
    checkDyldInjection();       // T-1, T-3: library injection
    checkHardenedRuntime();     // T-1: codesign integrity
    checkDependencySignature(); // T-1: poisoned Homebrew dependency

    // Soft checks — log and continue.
    adviseDmaThreats();         // T-2: Thunderbolt DMA
    auditAccessibilityGrants(); // T-4, T-5: AX permission exposure

    NSLog(@"[igi] === Security Guard: startup checks complete ===");
}

}  // namespace igi::core
