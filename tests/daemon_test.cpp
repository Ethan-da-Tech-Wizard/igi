// Program: igi - daemon_test.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Unit tests for Daemon covering its start/stop state, signals and idempotent start.

#include <gtest/gtest.h>

#include <QSignalSpy>

#include "core/Daemon.h"

using igi::core::Daemon;

// A new daemon is not running.
TEST(Daemon, StartsInStoppedState) {
    Daemon d;
    EXPECT_FALSE(d.isRunning());
}

// start() makes the daemon run and emits started.
TEST(Daemon, StartTransitionsRunningAndEmitsSignal) {
    Daemon d;
    QSignalSpy spy(&d, &Daemon::started);

    d.start();

    EXPECT_TRUE(d.isRunning());
    EXPECT_EQ(spy.count(), 1);
}

// Calling start() twice has the same effect as once.
TEST(Daemon, DoubleStartIsIdempotent) {
    Daemon d;
    QSignalSpy spy(&d, &Daemon::started);

    d.start();
    d.start();

    EXPECT_TRUE(d.isRunning());
    EXPECT_EQ(spy.count(), 1);
}

// stop() ends the running state and emits stopped.
TEST(Daemon, StopTransitionsAndEmitsSignal) {
    Daemon d;
    QSignalSpy spy(&d, &Daemon::stopped);

    d.start();
    d.stop();

    EXPECT_FALSE(d.isRunning());
    EXPECT_EQ(spy.count(), 1);
}

// The version string is never empty.
TEST(Daemon, VersionIsNonEmpty) {
    Daemon d;
    EXPECT_FALSE(d.version().isEmpty());
}
