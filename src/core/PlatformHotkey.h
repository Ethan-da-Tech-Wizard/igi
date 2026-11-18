// Program: igi - PlatformHotkey.h
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Defines the human-readable hotkey label strings used in the UI and logs for each platform.

#pragma once

#include <QString>

namespace igi::core {

#if defined(Q_OS_MACOS)
inline const QString kPlatformHotkeyLabel = QStringLiteral("Cmd+Ctrl+F");
inline const QString kPlatformHotkeyLog = QStringLiteral("Cmd+Ctrl+F");
#else
inline const QString kPlatformHotkeyLabel = QStringLiteral("Meta+Ctrl+F");
inline const QString kPlatformHotkeyLog = QStringLiteral("Meta+Ctrl+F");
#endif

} // namespace igi::core
