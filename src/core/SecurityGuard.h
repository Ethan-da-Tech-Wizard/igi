#pragma once

namespace igi::core {

// SecurityGuard runs a battery of startup security checks before any PHI
// can enter scope. If any hard check fails, the process aborts. Soft checks
// emit a warning but allow the process to continue (the risk is logged so
// an MDM syslog collector can catch it).
//
// Checks performed:
//   SEC-GUARD-01  DYLD_INSERT_LIBRARIES is not set (library injection).
//   SEC-GUARD-02  Hardened Runtime is active on our own binary.
//   SEC-GUARD-03  Key OCR dependency (libtesseract) passes code-signature
//                 validation — detects a poisoned Homebrew formula at
//                 runtime, not just at build time.
//   SEC-GUARD-04  Thunderbolt / DMA threat advisory (soft — logged only).
//   SEC-GUARD-05  No unexpected Accessibility-granted processes are running
//                 on this user session (soft — logged only, list is advisory).
class SecurityGuard {
public:
    // Run all checks. Hard failures abort the process. Soft warnings log to
    // stderr. Call this before initialising any other Igi component.
    static void runStartupChecks();

private:
    // Hard checks — abort on failure.
    static void checkDyldInjection();       // SEC-GUARD-01
    static void checkHardenedRuntime();     // SEC-GUARD-02
    static void checkDependencySignature(); // SEC-GUARD-03

    // Soft checks — warn only.
    static void adviseDmaThreats();         // SEC-GUARD-04
    static void auditAccessibilityGrants(); // SEC-GUARD-05
    static void checkFileVault();           // SEC-GUARD-06
};

}  // namespace igi::core
