#pragma once

#include <functional>
#include <memory>

namespace igi::core {

class IHotkeyListener {
public:
    virtual ~IHotkeyListener() = default;

    using Callback = std::function<void()>;

    // Registers the global Cmd+Shift+F hotkey. Returns true on success.
    // On macOS, this requires the Accessibility entitlement — call
    // preflightPermissions() first and surface the result to the user.
    virtual bool registerHotkey(Callback callback) = 0;

    virtual void unregisterHotkey() = 0;

    virtual bool isRegistered() const noexcept = 0;

    // Platform factory. Returns the NSEvent-based listener on macOS.
    static std::unique_ptr<IHotkeyListener> create();
};

}  // namespace igi::core
