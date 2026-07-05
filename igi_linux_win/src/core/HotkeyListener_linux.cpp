#include "core/HotkeyListener.h"

#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

// Undef X11 macros to prevent conflicts
#undef None
#undef Status
#undef Bool
#undef True
#undef False

namespace igi::core {

class X11HotkeyListener final : public IHotkeyListener {
public:
    X11HotkeyListener() = default;
    ~X11HotkeyListener() override { unregisterHotkey(); }

    bool registerHotkey(Callback callback) override {
        if (registered_) {
            return true;
        }

        callback_ = std::move(callback);
        running_ = true;

        if (pipe(pipeFds_) != 0) {
            return false;
        }

        // Spawn background thread to monitor events
        thread_ = std::thread(&X11HotkeyListener::runLoop, this);
        registered_ = true;
        return true;
    }

    void unregisterHotkey() override {
        if (!registered_) {
            return;
        }

        running_ = false;
        // Write to pipe to wake up poll()
        char dummy = 1;
        (void)::write(pipeFds_[1], &dummy, sizeof(dummy));

        if (thread_.joinable()) {
            thread_.join();
        }

        ::close(pipeFds_[0]);
        ::close(pipeFds_[1]);
        registered_ = false;
    }

    bool isRegistered() const noexcept override { return registered_; }

private:
    void runLoop() {
        Display* dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            std::cerr << "[igi] HotkeyListener: Cannot open X display.\n";
            return;
        }

        Window root = DefaultRootWindow(dpy);
        // Resolve keycode for '9' key
        KeyCode keycode = XKeysymToKeycode(dpy, XK_9);
        if (keycode == 0) {
            std::cerr << "[igi] HotkeyListener: Cannot map key '9'.\n";
            XCloseDisplay(dpy);
            return;
        }

        // Grab key Ctrl+Shift+9 with lock-modifier masks (CapsLock, NumLock) to be robust
        unsigned int modifiers[] = {
            0,
            Mod2Mask,             // NumLock
            LockMask,             // CapsLock
            Mod2Mask | LockMask
        };

        const unsigned int targetMods = ControlMask | ShiftMask;
        for (unsigned int mod : modifiers) {
            XGrabKey(dpy, keycode, targetMods | mod, root, 1, GrabModeAsync, GrabModeAsync);
        }

        int x11Fd = ConnectionNumber(dpy);
        struct pollfd fds[2];
        fds[0].fd = x11Fd;
        fds[0].events = POLLIN;
        fds[1].fd = pipeFds_[0];
        fds[1].events = POLLIN;

        while (running_) {
            int ret = poll(fds, 2, -1);
            if (ret < 0) {
                break; // error
            }

            if (fds[1].revents & POLLIN) {
                // Wake up for termination
                break;
            }

            if (fds[0].revents & POLLIN) {
                // X11 events available. Process all of them.
                while (XPending(dpy)) {
                    XEvent ev;
                    XNextEvent(dpy, &ev);
                    if (ev.type == KeyPress && ev.xkey.keycode == keycode) {
                        // Match base modifiers (ignoring Lock/NumLock)
                        unsigned int state = ev.xkey.state & ~(Mod2Mask | LockMask);
                        if (state == targetMods && callback_) {
                            callback_();
                        }
                    }
                }
            }
        }

        // Clean up grab
        for (unsigned int mod : modifiers) {
            XUngrabKey(dpy, keycode, targetMods | mod, root);
        }

        XCloseDisplay(dpy);
    }

    Callback          callback_;
    std::thread       thread_;
    std::atomic<bool> running_{false};
    bool              registered_{false};
    int               pipeFds_[2] = {-1, -1};
};

std::unique_ptr<IHotkeyListener> IHotkeyListener::create() {
    return std::make_unique<X11HotkeyListener>();
}

} // namespace igi::core
