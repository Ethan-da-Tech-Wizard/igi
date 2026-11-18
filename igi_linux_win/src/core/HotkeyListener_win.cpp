// Program: igi - HotkeyListener_win.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Windows global hotkey listener that registers a hotkey on a background thread running a message loop.

#include "core/HotkeyListener.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <atomic>
#include <iostream>

namespace igi::core {

// Registers the hotkey on a dedicated thread because Win32 hotkeys need a message loop on the registering thread.
class WinHotkeyListener final : public IHotkeyListener {
public:
    WinHotkeyListener() = default;
    ~WinHotkeyListener() override { unregisterHotkey(); }

    bool registerHotkey(Callback callback) override {
        if (registered_) {
            return true;
        }

        callback_ = std::move(callback);
        running_ = true;
        registeredSuccess_ = false;

        // Start the message-loop thread, then wait until it reports success or failure.
        thread_ = std::thread(&WinHotkeyListener::runLoop, this);

        // Spin wait briefly for thread to register hotkey
        while (running_ && !registeredSuccess_) {
            std::this_thread::yield();
        }

        registered_ = registeredSuccess_;
        return registered_;
    }

    void unregisterHotkey() override {
        if (!registered_) {
            return;
        }

        running_ = false;
        if (threadId_ != 0) {
            PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        }

        if (thread_.joinable()) {
            thread_.join();
        }

        registered_ = false;
        threadId_ = 0;
    }

    bool isRegistered() const noexcept override { return registered_; }

private:
    // Thread body: registers the hotkey, then dispatches the callback for each hotkey message until quit.
    void runLoop() {
        threadId_ = GetCurrentThreadId();

        // Register hotkey: Ctrl+Shift+9 (0x39 is Virtual Key code for '9')
        BOOL ok = RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_SHIFT, 0x39);
        if (!ok) {
            std::cerr << "[igi] HotkeyListener: RegisterHotKey failed (error: " << GetLastError() << ")\n";
            running_ = false;
            return;
        }

        registeredSuccess_ = true;

        MSG msg;
        while (running_ && GetMessageW(&msg, nullptr, 0, 0)) {
            if (msg.message == WM_HOTKEY && msg.wParam == 1) {
                if (callback_) {
                    callback_();
                }
            }
        }

        UnregisterHotKey(nullptr, 1);
    }

    Callback           callback_;
    std::thread        thread_;
    std::atomic<bool>  running_{false};
    std::atomic<bool>  registeredSuccess_{false};
    std::atomic<DWORD> threadId_{0};
    bool               registered_{false};
};

// Factory: the Windows build always returns this listener.
std::unique_ptr<IHotkeyListener> IHotkeyListener::create() {
    return std::make_unique<WinHotkeyListener>();
}

} // namespace igi::core
