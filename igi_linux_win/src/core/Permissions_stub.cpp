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
