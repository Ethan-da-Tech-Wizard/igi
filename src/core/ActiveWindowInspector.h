#pragma once

#include <optional>

#include <QRect>
#include <QString>

#if defined(Q_OS_WIN)
using pid_t = qint64;
#else
#include <sys/types.h>
#endif

namespace igi::core {

struct ActiveWindowInfo {
    QString  bundleId;       // e.g., "com.apple.Preview"
    QString  appName;        // e.g., "Preview"
    QRect    bounds;         // virtual-desktop point coords (top-left origin)
    QString  documentPath;   // populated for document-bearing windows; empty otherwise
    pid_t    pid = 0;
};

// Queries the macOS Accessibility API for the focused window across the
// whole system. Requires the Accessibility entitlement (Chunk 1 already
// gates on this). Returns nullopt if no focused window is reachable
// within the AX message timeout.
//
// AX queries can block waiting for the target app to respond — we set
// a tight timeout so a busy foreign app doesn't blow our latency SLO
// (DECISIONS.md D-002).
std::optional<ActiveWindowInfo> queryActiveWindow();

}  // namespace igi::core
