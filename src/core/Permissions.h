#pragma once

namespace igi::core {

struct PermissionStatus {
    bool screenRecording = false;
    bool accessibility   = false;

    bool allGranted() const noexcept { return screenRecording && accessibility; }
};

// Checks current TCC grant status without triggering OS prompts.
PermissionStatus preflightPermissions();

// Opens the relevant pane in System Settings so the user can grant access.
void openScreenRecordingSettings();
void openAccessibilitySettings();

}  // namespace igi::core
