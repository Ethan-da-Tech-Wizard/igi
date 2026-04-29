#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "core/HotkeyListener.h"
#include "core/Permissions.h"

namespace igi::core {

class Daemon : public QObject {
    Q_OBJECT

public:
    // Dependency injection: pass a custom listener for testing.
    // When nullptr, the platform default (NSEventHotkeyListener) is used.
    explicit Daemon(
        std::unique_ptr<IHotkeyListener> listener = nullptr,
        QObject* parent = nullptr);

    ~Daemon() override;

    bool isRunning()      const noexcept { return running_; }
    bool hotkeyActive()   const noexcept { return listener_ && listener_->isRegistered(); }
    PermissionStatus permissions() const noexcept { return permissions_; }

    QString version() const;

public slots:
    void start();
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
