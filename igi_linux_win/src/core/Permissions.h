// Program: igi - Permissions.h
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares the permission status struct and helpers for checking and opening OS permission settings.

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
