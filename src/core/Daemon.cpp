// ==============================================================================
// Program: Daemon.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Background service object: runs the macOS/Linux/Windows permission preflight and owns the global hotkey listener, emitting hotkeyTriggered() when the shortcut is pressed.
//          (Single-instance locking and the local IPC socket are NOT here; they live in main.cpp.)
// ==============================================================================

#include "core/Daemon.h"

#include <QDebug>

namespace igi::core {

/**
 * @param listener Hotkey backend; nullptr selects the platform default.
 * @param parent   Optional QObject owner.
 */
Daemon::Daemon(std::unique_ptr<IHotkeyListener> listener, QObject* parent)
    : QObject(parent)
    , listener_(listener ? std::move(listener) : IHotkeyListener::create())
{}

/** Releases the OS hotkey if the daemon is destroyed while running. */
Daemon::~Daemon() {
    // Ensure the OS hotkey is released even if the daemon is destroyed
    // while running (e.g., abnormal shutdown). We do not call stop()
    // here because it would emit `stopped` during destruction, which is
    // unsafe for any receiver whose lifetime is tied to this Daemon.
    if (listener_) {
        listener_->unregisterHotkey();
    }
}

/** Fixed placeholder version string ("chunk1" is the development milestone name). */
QString Daemon::version() const {
    return QStringLiteral("0.1.0-chunk1");
}

/**
 * Starts the daemon once: permission preflight, hotkey registration and the
 * started() signal. Calling it again while running does nothing.
 */
void Daemon::start() {
    if (running_) {
        return;
    }
    running_ = true;
    checkAndWirePermissions();
    emit started();
}

/**
 * Stops the daemon: unregisters the hotkey and emits stopped(). Does nothing
 * if it is not running.
 */
void Daemon::stop() {
    if (!running_) {
        return;
    }
    if (listener_) {
        listener_->unregisterHotkey();
    }
    running_ = false;
    emit stopped();
}

/**
 * Preflights the macOS permissions (Screen Recording for capture, Accessibility
 * for the global hotkey), logs what is missing, tells subscribers through
 * permissionsChecked() and then tries to register the hotkey anyway.
 * NOTE: the success message below still says "Cmd+Shift+9", an old hotkey; the
 * real shortcut is kPlatformHotkeyLabel (PlatformHotkey.h).
 */
void Daemon::checkAndWirePermissions() {
    permissions_ = preflightPermissions();

    if (!permissions_.screenRecording) {
        qWarning() << "[igi] Screen Recording permission not granted. "
                      "Visual capture will fail. "
                      "Open System Settings > Privacy & Security > Screen Recording.";
    }

    if (!permissions_.accessibility) {
        qWarning() << "[igi] Accessibility permission not granted. "
                      "Global hotkey registration will fail. "
                      "Open System Settings > Privacy & Security > Accessibility.";
    }

    emit permissionsChecked(permissions_.screenRecording, permissions_.accessibility);

    // Registration is attempted even when a permission is missing: without
    // Accessibility it simply fails and we continue with the warning above
    // (the user can grant the permission and restart).
    // Attempt hotkey registration regardless — on denial the listener returns
    // false and isRegistered() stays false. We log but do not crash.
    bool ok = listener_->registerHotkey([this] { emit hotkeyTriggered(); });
    if (!ok) {
        qWarning() << "[igi] Global hotkey registration failed "
                      "(check Accessibility permission).";
    } else {
        qDebug() << "[igi] Global hotkey Cmd+Shift+9 registered.";
    }
}

}  // namespace igi::core
