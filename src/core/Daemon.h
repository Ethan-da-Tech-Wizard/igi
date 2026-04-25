#pragma once

#include <QObject>
#include <QString>

namespace igi::core {

class Daemon : public QObject {
    Q_OBJECT

public:
    explicit Daemon(QObject* parent = nullptr);
    ~Daemon() override;

    bool isRunning() const noexcept { return running_; }

    QString version() const;

public slots:
    void start();
    void stop();

signals:
    void started();
    void stopped();

private:
    bool running_ = false;
};

}  // namespace igi::core
