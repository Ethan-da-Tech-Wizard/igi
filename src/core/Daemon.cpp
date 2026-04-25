#include "core/Daemon.h"

namespace igi::core {

Daemon::Daemon(QObject* parent) : QObject(parent) {}

Daemon::~Daemon() = default;

QString Daemon::version() const {
    return QStringLiteral("0.1.0-chunk0");
}

void Daemon::start() {
    if (running_) {
        return;
    }
    running_ = true;
    emit started();
}

void Daemon::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    emit stopped();
}

}  // namespace igi::core
