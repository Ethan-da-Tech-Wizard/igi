// Program: igi - ActiveWindowInspector_win.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Windows implementation of queryActiveWindow using Win32 foreground-window, DWM and process APIs.

#include "core/ActiveWindowInspector.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <dwmapi.h>

#include <QRect>
#include <QString>
#include <QFileInfo>
#include <vector>

namespace igi::core {

// Returns details of the foreground window, or nullopt when there is none.
std::optional<ActiveWindowInfo> queryActiveWindow() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return std::nullopt;
    }

    ActiveWindowInfo info;
    
    // Query process ID
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    info.pid = static_cast<pid_t>(pid);

    // Query window bounds (preferring DWM frame bounds to avoid hidden drop shadow margins)
    RECT rect;
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect)))) {
        GetWindowRect(hwnd, &rect);
    }
    info.bounds = QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

    // Query active process path to populate appName and bundleId
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        wchar_t path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            QString fullPath = QString::fromWCharArray(path);
            QFileInfo fileInfo(fullPath);
            info.appName = fileInfo.baseName();
            info.bundleId = fileInfo.fileName();
        }
        CloseHandle(process);
    }

    // Fallback: use the window text if executable name query failed
    if (info.appName.isEmpty()) {
        int len = GetWindowTextLengthW(hwnd);
        if (len > 0) {
            std::vector<wchar_t> buf(len + 1);
            GetWindowTextW(hwnd, buf.data(), len + 1);
            info.appName = QString::fromWCharArray(buf.data());
        }
    }

    // documentPath is left blank on Windows and falls back to window capture ingestion
    info.documentPath = "";

    return info;
}

} // namespace igi::core
