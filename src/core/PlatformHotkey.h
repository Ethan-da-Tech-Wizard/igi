// ==============================================================================
// Program: PlatformHotkey.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Defines platform-agnostic keyboard shortcut representations, modifier masks, and chord definitions across macOS, Linux, and Windows desktop environments.
// ==============================================================================

#pragma once

#include <QString>

// Text shown to the user (tray menu entry "Hotkey: ...", see main.cpp) and in
// the start-up log. NOTE: these strings are only labels; the real shortcut is
// set in each HotkeyListener implementation and currently differs: macOS
// registers Cmd+Shift+9 (HotkeyListener.mm), Windows Ctrl+Shift+9
// (HotkeyListener_win.cpp), Linux Meta+Ctrl+F (HotkeyListener_linux.cpp). So
// the label is wrong on macOS and Windows.
namespace igi::core {

#if defined(Q_OS_MACOS)
inline const QString kPlatformHotkeyLabel = QStringLiteral("Cmd+Ctrl+F");
inline const QString kPlatformHotkeyLog = QStringLiteral("Cmd+Ctrl+F");
#else
inline const QString kPlatformHotkeyLabel = QStringLiteral("Meta+Ctrl+F");
inline const QString kPlatformHotkeyLog = QStringLiteral("Meta+Ctrl+F");
#endif

} // namespace igi::core
