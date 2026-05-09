#include "core/HotkeyListener.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

namespace igi::core {

// ---------------------------------------------------------------------------
// NSEventHotkeyListener — primary path (requires Accessibility entitlement)
// ---------------------------------------------------------------------------
class NSEventHotkeyListener final : public IHotkeyListener {
public:
    ~NSEventHotkeyListener() override { unregisterHotkey(); }

    bool registerHotkey(Callback callback) override {
        if (registered_) {
            return true;
        }

        callback_ = std::move(callback);

        // Global monitor fires when ANY other app has focus.
        // kVK_ANSI_F = 0x03 (Carbon virtual key for F on all layouts).
        const NSUInteger requiredMods =
            NSEventModifierFlagCommand | NSEventModifierFlagShift;

        globalMonitor_ = [NSEvent
            addGlobalMonitorForEventsMatchingMask:NSEventMaskKeyDown
            handler:^(NSEvent* event) {
                if ((event.modifierFlags & requiredMods) == requiredMods &&
                    event.keyCode == kVK_ANSI_9) {
                    if (callback_) callback_();
                }
            }];

        // Local monitor fires when Igi itself has focus (e.g. search bar open).
        localMonitor_ = [NSEvent
            addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
            handler:^NSEvent*(NSEvent* event) {
                if ((event.modifierFlags & requiredMods) == requiredMods &&
                    event.keyCode == kVK_ANSI_9) {
                    if (callback_) callback_();
                    return nil;  // consume the event
                }
                return event;
            }];

        // addGlobalMonitor silently returns nil when Accessibility is denied.
        registered_ = (globalMonitor_ != nil);
        return registered_;
    }

    void unregisterHotkey() override {
        if (globalMonitor_) {
            [NSEvent removeMonitor:globalMonitor_];
            globalMonitor_ = nil;
        }
        if (localMonitor_) {
            [NSEvent removeMonitor:localMonitor_];
            localMonitor_ = nil;
        }
        registered_ = false;
    }

    bool isRegistered() const noexcept override { return registered_; }

private:
    Callback  callback_;
    id        globalMonitor_ = nil;
    id        localMonitor_  = nil;
    bool      registered_    = false;
};

// ---------------------------------------------------------------------------
// CarbonHotkeyListener — compiled fallback (see DECISIONS.md D-004)
// Activate by changing IHotkeyListener::create() to return this type.
// ---------------------------------------------------------------------------
class CarbonHotkeyListener final : public IHotkeyListener {
public:
    ~CarbonHotkeyListener() override { unregisterHotkey(); }

    bool registerHotkey(Callback callback) override {
        if (registered_) return true;

        callback_ = std::move(callback);

        EventHotKeyID keyID{.signature = 'IGI1', .id = 1};
        OSStatus status = RegisterEventHotKey(
            kVK_ANSI_9,
            cmdKey | shiftKey,
            keyID,
            GetApplicationEventTarget(),
            0,
            &hotKeyRef_);

        if (status != noErr) return false;

        // Install event handler to receive the hotkey event.
        EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
        InstallApplicationEventHandler(
            NewEventHandlerUPP([](EventHandlerCallRef, EventRef, void* ctx) -> OSStatus {
                auto* self = static_cast<CarbonHotkeyListener*>(ctx);
                if (self->callback_) self->callback_();
                return noErr;
            }),
            1, &spec, this, &handlerRef_);

        registered_ = true;
        return true;
    }

    void unregisterHotkey() override {
        if (hotKeyRef_) {
            UnregisterEventHotKey(hotKeyRef_);
            hotKeyRef_ = nullptr;
        }
        if (handlerRef_) {
            RemoveEventHandler(handlerRef_);
            handlerRef_ = nullptr;
        }
        registered_ = false;
    }

    bool isRegistered() const noexcept override { return registered_; }

private:
    Callback           callback_;
    EventHotKeyRef     hotKeyRef_  = nullptr;
    EventHandlerRef    handlerRef_ = nullptr;
    bool               registered_ = false;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
std::unique_ptr<IHotkeyListener> IHotkeyListener::create() {
    return std::make_unique<CarbonHotkeyListener>();
}

}  // namespace igi::core
