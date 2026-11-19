// ==============================================================================
// Program: HotkeyListener.mm
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Installs low-level system keyboard hooks and event taps to capture global shortcut combinations (e.g. Cmd+Shift+F / Ctrl+Shift+F) to trigger instant search sessions.
// ==============================================================================

#include "core/HotkeyListener.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

namespace igi::core {

// ---------------------------------------------------------------------------
// NSEventHotkeyListener — primary path (requires Accessibility entitlement)
// ---------------------------------------------------------------------------
/**
 * Global hotkey through NSEvent monitors (macOS). Needs the Accessibility
 * permission: without it addGlobalMonitor returns nil and registration fails.
 * Hotkey: Cmd+Shift+9. A global monitor sees keys while OTHER apps have focus;
 * a local monitor handles the same key while Igi itself is frontmost.
 */
class NSEventHotkeyListener final : public IHotkeyListener {
public:
    ~NSEventHotkeyListener() override { unregisterHotkey(); }

    bool registerHotkey(Callback callback) override {
        if (registered_) {
            return true;
        }

        callback_ = std::move(callback);

        // Global monitor fires when ANY other app has focus.
        // The key is kVK_ANSI_9 (Carbon virtual key code for "9", the same on all
        // keyboard layouts). Earlier comments and docs say Cmd+Shift+F, which is
        // not what the code registers: the real shortcut is Cmd+Shift+9.
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
/**
 * Alternative backend using Carbon RegisterEventHotKey (no Accessibility
 * permission needed). Compiled but unused unless IHotkeyListener::create() is
 * changed to return it (docs/DECISIONS.md D-004). Same shortcut: Cmd+Shift+9.
 */
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
