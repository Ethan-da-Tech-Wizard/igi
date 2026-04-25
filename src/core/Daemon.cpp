#include "core/Daemon.h"

#include <QDebug>

namespace igi::core {

Daemon::Daemon(std::unique_ptr<IHotkeyListener> listener, QObject* parent)
    : QObject(parent)
    , listener_(listener ? std::move(listener) : IHotkeyListener::create())
{}

Daemon::~Daemon() = default;

QString Daemon::version() const {
    return QStringLiteral("0.1.0-chunk1");
}

void Daemon::start() {
    if (running_) {
        return;
    }
    running_ = true;
    checkAndWirePermissions();
    emit started();
}

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

    if (!permissions_.allGranted()) {
        emit permissionsMissing(permissions_.screenRecording, permissions_.accessibility);
    }

    // Attempt hotkey registration regardless — on denial the listener returns
    // false and isRegistered() stays false. We log but do not crash.
    bool ok = listener_->registerHotkey([this] { emit hotkeyTriggered(); });
    if (!ok) {
        qWarning() << "[igi] Global hotkey registration failed "
                      "(check Accessibility permission).";
    } else {
        qDebug() << "[igi] Global hotkey Cmd+Shift+F registered.";
    }
}

}  // namespace igi::core
