#pragma once

#include <QString>

namespace igi::core {

#if defined(Q_OS_MACOS)
inline const QString kPlatformHotkeyLabel = QStringLiteral("Cmd+Shift+9");
inline const QString kPlatformHotkeyLog = QStringLiteral("Cmd+Shift+9");
#else
inline const QString kPlatformHotkeyLabel = QStringLiteral("Ctrl+Shift+9");
inline const QString kPlatformHotkeyLog = QStringLiteral("Ctrl+Shift+9");
#endif

} // namespace igi::core
