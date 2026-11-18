// Program: igi - Daemon.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Implements Daemon start/stop, permission preflight reporting and global hotkey registration.

#include "core/Daemon.h"

#include <QDebug>

namespace igi::core {

// Uses the supplied listener, or creates the platform default when none is given.
Daemon::Daemon(std::unique_ptr<IHotkeyListener> listener, QObject* parent)
    : QObject(parent)
    , listener_(listener ? std::move(listener) : IHotkeyListener::create())
{}

Daemon::~Daemon() {
    // Ensure the OS hotkey is released even if the daemon is destroyed
    // while running (e.g., abnormal shutdown). We do not call stop()
    // here because it would emit `stopped` during destruction, which is
    // unsafe for any receiver whose lifetime is tied to this Daemon.
    if (listener_) {
        listener_->unregisterHotkey();
    }
}

// Returns the daemon's version string.
QString Daemon::version() const {
    return QStringLiteral("0.1.0-chunk1");
}

// Starts the daemon once: checks permissions, registers the hotkey and emits started().
void Daemon::start() {
    if (running_) {
        return;
    }
    running_ = true;
    checkAndWirePermissions();
    emit started();
}

// Releases the hotkey and emits stopped(); does nothing if not running.
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

// Reads permission state, warns about gaps, reports them and then registers the hotkey.
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
