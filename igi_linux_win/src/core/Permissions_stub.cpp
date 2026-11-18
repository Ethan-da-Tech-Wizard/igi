// ==============================================================================
// Program: Permissions_stub.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Checks and requests platform-level screen recording and accessibility permissions, guiding users through system privacy authorizations required for desktop capture.
// ==============================================================================

#include "core/Permissions.h"

namespace igi::core {

PermissionStatus preflightPermissions() {
    PermissionStatus status;
    status.screenRecording = true;
    status.accessibility   = true;
    return status;
}

void openScreenRecordingSettings() {}
void openAccessibilitySettings() {}

} // namespace igi::core
