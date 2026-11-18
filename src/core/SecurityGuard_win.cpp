// Program: igi - SecurityGuard_win.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Windows SecurityGuard implementation: most checks are stubs and it prints a disk-encryption advisory.

#include "core/SecurityGuard.h"
#include <cstdlib>
#include <iostream>

namespace igi::core {

void SecurityGuard::checkDyldInjection() {
    // Stubbed on Windows.
}

void SecurityGuard::checkHardenedRuntime() {
    // Stubbed on Windows.
}

void SecurityGuard::checkDependencySignature() {
    // Stubbed on Windows.
}

void SecurityGuard::adviseDmaThreats() {
    // Stubbed on Windows.
}

void SecurityGuard::auditAccessibilityGrants() {
    // Stubbed on Windows.
}

// Advisory only: reminds the user to enable drive encryption.
void SecurityGuard::checkFileVault() {
    std::cout << "[igi] SECURITY ADVISORY: Ensure BitLocker disk encryption is enabled "
                 "on your system drive to protect memory swap files containing PHI.\n";
}

// Runs each check in order and prints start and finish banners.
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
