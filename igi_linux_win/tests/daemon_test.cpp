#include <gtest/gtest.h>

#include <QSignalSpy>

#include "core/Daemon.h"

using igi::core::Daemon;

TEST(Daemon, StartsInStoppedState) {
    Daemon d;
    EXPECT_FALSE(d.isRunning());
}

TEST(Daemon, StartTransitionsRunningAndEmitsSignal) {
    Daemon d;
    QSignalSpy spy(&d, &Daemon::started);

    d.start();

    EXPECT_TRUE(d.isRunning());
    EXPECT_EQ(spy.count(), 1);
}

TEST(Daemon, DoubleStartIsIdempotent) {
    Daemon d;
    QSignalSpy spy(&d, &Daemon::started);

    d.start();
    d.start();

    EXPECT_TRUE(d.isRunning());
    EXPECT_EQ(spy.count(), 1);
}

TEST(Daemon, StopTransitionsAndEmitsSignal) {
    Daemon d;
    QSignalSpy spy(&d, &Daemon::stopped);

    d.start();
    d.stop();

    EXPECT_FALSE(d.isRunning());
    EXPECT_EQ(spy.count(), 1);
}

TEST(Daemon, VersionIsNonEmpty) {
    Daemon d;
    EXPECT_FALSE(d.version().isEmpty());
}
