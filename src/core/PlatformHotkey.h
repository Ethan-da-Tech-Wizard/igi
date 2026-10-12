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
