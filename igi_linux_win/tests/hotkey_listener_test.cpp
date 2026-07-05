#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <QSignalSpy>

#include "core/HotkeyListener.h"
#include "core/Daemon.h"

using namespace igi::core;
using ::testing::_;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Test double: a controllable IHotkeyListener that records calls and lets
// tests fire the callback manually without needing real OS permissions.
// ---------------------------------------------------------------------------
class MockHotkeyListener final : public IHotkeyListener {
public:
    MOCK_METHOD(bool, registerHotkey, (Callback callback), (override));
    MOCK_METHOD(void, unregisterHotkey, (), (override));
    MOCK_METHOD(bool, isRegistered, (), (const, noexcept, override));

    // Convenience: captures the callback and exposes it so tests can fire it.
    bool captureAndRegister(Callback callback) {
        captured_ = std::move(callback);
        registered_ = true;
        return true;
    }

    void fireCallback() {
        if (captured_) captured_();
    }

    bool registeredState() const { return registered_; }

private:
    Callback captured_;
    bool     registered_ = false;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
TEST(HotkeyListener, DaemonStartRegistersHotkey) {
    auto* raw = new MockHotkeyListener();
    EXPECT_CALL(*raw, registerHotkey(_)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*raw, isRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*raw, unregisterHotkey()).Times(testing::AtLeast(1));

    Daemon daemon{std::unique_ptr<IHotkeyListener>(raw)};
    daemon.start();

    EXPECT_TRUE(daemon.isRunning());
    EXPECT_TRUE(daemon.hotkeyActive());
}

TEST(HotkeyListener, HotkeyCallbackEmitsDaemonSignal) {
    auto* raw = new MockHotkeyListener();

    // Capture the registered callback so we can fire it manually.
    IHotkeyListener::Callback captured;
    EXPECT_CALL(*raw, registerHotkey(_))
        .WillOnce([&](IHotkeyListener::Callback cb) {
            captured = std::move(cb);
            return true;
        });
    EXPECT_CALL(*raw, isRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*raw, unregisterHotkey()).Times(testing::AtLeast(1));

    Daemon daemon{std::unique_ptr<IHotkeyListener>(raw)};
    QSignalSpy spy(&daemon, &Daemon::hotkeyTriggered);

    daemon.start();
    ASSERT_TRUE(captured) << "registerHotkey was not called with a callback";

    captured();  // simulate Cmd+Shift+F

    EXPECT_EQ(spy.count(), 1);
}

TEST(HotkeyListener, DaemonStopUnregistersHotkey) {
    auto* raw = new MockHotkeyListener();
    EXPECT_CALL(*raw, registerHotkey(_)).WillOnce(Return(true));
    EXPECT_CALL(*raw, isRegistered()).WillRepeatedly(Return(false));
    EXPECT_CALL(*raw, unregisterHotkey()).Times(testing::AtLeast(1));

    Daemon daemon{std::unique_ptr<IHotkeyListener>(raw)};
    daemon.start();
    daemon.stop();

    EXPECT_FALSE(daemon.isRunning());
}

TEST(HotkeyListener, RegistrationFailureDoesNotCrashDaemon) {
    auto* raw = new MockHotkeyListener();
    // Simulates missing Accessibility permission.
    EXPECT_CALL(*raw, registerHotkey(_)).WillOnce(Return(false));
    EXPECT_CALL(*raw, isRegistered()).WillRepeatedly(Return(false));
    EXPECT_CALL(*raw, unregisterHotkey()).Times(testing::AtLeast(1));

    Daemon daemon{std::unique_ptr<IHotkeyListener>(raw)};
    daemon.start();

    EXPECT_TRUE(daemon.isRunning());
    EXPECT_FALSE(daemon.hotkeyActive());
}

TEST(HotkeyListener, PermissionsCheckedSignalFiresOnEveryStart) {
    auto* raw = new MockHotkeyListener();
    EXPECT_CALL(*raw, registerHotkey(_)).WillOnce(Return(true));
    EXPECT_CALL(*raw, isRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*raw, unregisterHotkey()).Times(testing::AtLeast(1));

    Daemon daemon{std::unique_ptr<IHotkeyListener>(raw)};
    QSignalSpy spy(&daemon, &Daemon::permissionsChecked);

    daemon.start();

    // permissionsChecked is design-spec'd to fire exactly once per start(),
    // regardless of whether perms are granted or denied — that's how
    // subscribers (like main.cpp's dialog) decide whether to act.
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0].size(), 2) << "signal must carry the (screen, ax) pair";
}

TEST(HotkeyListener, RepeatedStartDoesNotRefirePermissionsCheck) {
    auto* raw = new MockHotkeyListener();
    EXPECT_CALL(*raw, registerHotkey(_)).WillOnce(Return(true));
    EXPECT_CALL(*raw, isRegistered()).WillRepeatedly(Return(true));
    EXPECT_CALL(*raw, unregisterHotkey()).Times(testing::AtLeast(1));

    Daemon daemon{std::unique_ptr<IHotkeyListener>(raw)};
    QSignalSpy spy(&daemon, &Daemon::permissionsChecked);

    daemon.start();
    daemon.start();  // idempotent — should not re-run preflight

    EXPECT_EQ(spy.count(), 1);
}
