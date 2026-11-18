// ==============================================================================
// Program: daemon_test.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Provides single-instance application lifecycle locking and local IPC communication via local domain sockets, routing activation signals to the active daemon process.
// ==============================================================================

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
