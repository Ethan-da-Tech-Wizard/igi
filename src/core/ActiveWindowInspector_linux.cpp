#include "core/ActiveWindowInspector.h"

#include <QDir>
#include <QFile>
#include <QRect>
#include <QString>
#include <QVariant>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

// Prevent X11 macro pollution from breaking Qt and standard library
#undef None
#undef Status
#undef Bool
#undef True
#undef False

namespace igi::core {

std::optional<ActiveWindowInfo> queryActiveWindow() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        return std::nullopt;
    }

    Window root = DefaultRootWindow(dpy);
    Window active_win = 0;

    // Get active window using _NET_ACTIVE_WINDOW property on the root window
    Atom active_window_atom = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", 0);
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop = nullptr;

    if (XGetWindowProperty(dpy, root, active_window_atom, 0, 1, 0, XA_WINDOW,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == 0 && prop) {
        if (nitems > 0) {
            active_win = *reinterpret_cast<Window*>(prop);
        }
        XFree(prop);
    }

    // Fallback: request input focus
    if (active_win == 0) {
        int revert_to = 0;
        XGetInputFocus(dpy, &active_win, &revert_to);
    }

    if (active_win == 0 || active_win == root) {
        XCloseDisplay(dpy);
        return std::nullopt;
    }

    // Query window attributes (geometry)
    XWindowAttributes attr;
    if (XGetWindowAttributes(dpy, active_win, &attr) == 0) {
        XCloseDisplay(dpy);
        return std::nullopt;
    }

    // Translate window-relative origin (0, 0) to root-relative coordinates
    int abs_x = 0, abs_y = 0;
    Window child = 0;
    XTranslateCoordinates(dpy, active_win, root, 0, 0, &abs_x, &abs_y, &child);

    ActiveWindowInfo info;
    info.bounds = QRect(abs_x, abs_y, attr.width, attr.height);

    // Query window process ID via _NET_WM_PID
    Atom pid_atom = XInternAtom(dpy, "_NET_WM_PID", 0);
    prop = nullptr;
    if (XGetWindowProperty(dpy, active_win, pid_atom, 0, 1, 0, XA_CARDINAL,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == 0 && prop) {
        if (nitems > 0) {
            info.pid = static_cast<pid_t>(*reinterpret_cast<unsigned long*>(prop));
        }
        XFree(prop);
    }

    // Query class names for App name & Bundle ID mapping
    XClassHint classHint;
    if (XGetClassHint(dpy, active_win, &classHint) != 0) {
        if (classHint.res_class) {
            info.appName = QString::fromUtf8(classHint.res_class);
            XFree(classHint.res_class);
        }
        if (classHint.res_name) {
            info.bundleId = QString::fromUtf8(classHint.res_name);
            XFree(classHint.res_name);
        }
    }

    // Fallback: use window title if appName is still blank
    if (info.appName.isEmpty()) {
        Atom name_atom = XInternAtom(dpy, "_NET_WM_NAME", 0);
        prop = nullptr;
        if (XGetWindowProperty(dpy, active_win, name_atom, 0, 1024, 0, XInternAtom(dpy, "UTF8_STRING", 0),
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop) == 0 && prop) {
            if (nitems > 0) {
                info.appName = QString::fromUtf8(reinterpret_cast<char*>(prop));
            }
            XFree(prop);
        }
    }

    XCloseDisplay(dpy);

    // Read open file descriptors of the PID to detect if a PDF is opened
    if (info.pid > 0) {
        QDir fdDir(QStringLiteral("/proc/%1/fd").arg(info.pid));
        if (fdDir.exists()) {
            QStringList entries = fdDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QString& entry : entries) {
                QString target = QFile::symLinkTarget(fdDir.absoluteFilePath(entry));
                if (target.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
                    info.documentPath = target;
                    break;
                }
            }
        }
    }

    return info;
}

} // namespace igi::core
