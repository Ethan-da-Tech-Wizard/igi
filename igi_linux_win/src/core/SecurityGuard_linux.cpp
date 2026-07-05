#include "core/SecurityGuard.h"
#include <cstdlib>
#include <iostream>

namespace igi::core {

void SecurityGuard::checkDyldInjection() {
    if (std::getenv("LD_PRELOAD") != nullptr ||
        std::getenv("LD_LIBRARY_PATH") != nullptr) {
        std::cerr << "[igi] SECURITY WARNING: LD_PRELOAD or LD_LIBRARY_PATH is set. "
                     "Verify that the environment is trusted.\n";
    }
}

void SecurityGuard::checkHardenedRuntime() {
    // No Hardened Runtime equivalent natively on Linux; stubbed.
}

void SecurityGuard::checkDependencySignature() {
    // No native dylib signature checking API on Linux; stubbed.
}

void SecurityGuard::adviseDmaThreats() {
    // Thunderbolt DMA advisories are macOS specific in this implementation; stubbed.
}

void SecurityGuard::auditAccessibilityGrants() {
    // Accessibility permissions are macOS TCC specific; stubbed.
}

void SecurityGuard::checkFileVault() {
    // Advisory check for full disk encryption (LUKS / dm-crypt).
    std::cout << "[igi] SECURITY ADVISORY: Ensure LUKS / dm-crypt disk encryption is enabled "
                 "on your root partition to secure swap files containing PHI.\n";
}

void SecurityGuard::runStartupChecks() {
    std::cout << "[igi] === Security Guard: running startup checks ===\n";
    checkDyldInjection();
    checkHardenedRuntime();
    checkDependencySignature();
    adviseDmaThreats();
    auditAccessibilityGrants();
    checkFileVault();
    std::cout << "[igi] === Security Guard: startup checks complete ===\n";
}

} // namespace igi::core
