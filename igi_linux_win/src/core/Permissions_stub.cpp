// Program: igi - Permissions_stub.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Permission stub for non-macOS builds that reports every permission as granted and opens nothing.

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
