// ==============================================================================
// Program: Daemon.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Background service object: runs the macOS/Linux/Windows permission preflight and owns the global hotkey listener, emitting hotkeyTriggered() when the shortcut is pressed.
//          (Single-instance locking and the local IPC socket are NOT here; they live in main.cpp.)
// ==============================================================================

#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "core/HotkeyListener.h"
#include "core/Permissions.h"

namespace igi::core {

/**
 * Lifecycle owner of the global hotkey.
 *
 * start(): preflight permissions, emit permissionsChecked(), register the
 * hotkey, emit started(). stop(): release the hotkey, emit stopped().
 * Hotkey presses arrive as hotkeyTriggered(), which main.cpp wires to
 * SearchSession::activate(). Lives on the GUI thread; platform specifics are
 * hidden behind IHotkeyListener.
 */
class Daemon : public QObject {
    Q_OBJECT

public:
    // Dependency injection: pass a custom listener for testing (see
    // tests/daemon_test.cpp). When nullptr, the platform default from
    // IHotkeyListener::create() is used.
    explicit Daemon(
        std::unique_ptr<IHotkeyListener> listener = nullptr,
        QObject* parent = nullptr);

    ~Daemon() override;

    // True between start() and stop().
    bool isRunning()      const noexcept { return running_; }
    // True if the OS accepted the hotkey registration.
    bool hotkeyActive()   const noexcept { return listener_ && listener_->isRegistered(); }
    // Result of the last permission preflight (Screen Recording, Accessibility).
    PermissionStatus permissions() const noexcept { return permissions_; }

    // Version string for diagnostics (currently a fixed placeholder).
    QString version() const;

public slots:
    // Idempotent. Preflights permissions, emits started() and registers the hotkey.
    void start();
    // Idempotent. Unregisters the hotkey and emits stopped().
    void stop();

signals:
    void started();
    void stopped();
    void hotkeyTriggered();
    // Fires once per start() after preflight, with the granted state of each
    // permission. Subscribers decide what to do (e.g., show a dialog if any
    // are missing). Naming is neutral on purpose — both the "all granted"
    // and "something missing" listeners use the same payload.
    void permissionsChecked(bool screenRecordingGranted, bool accessibilityGranted);

private:
    void checkAndWirePermissions();

    std::unique_ptr<IHotkeyListener> listener_;
    PermissionStatus                 permissions_{};
    bool                             running_ = false;
};

}  // namespace igi::core
