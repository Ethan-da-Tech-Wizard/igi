// ==============================================================================
// Program: main.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Application entry point initializing Qt event loops, signal handlers, local IPC listeners, system tray notifications, global hotkeys, and core background services.
//
// How it fits: everything starts here. main() wires the long-lived objects
// together and then hands control to the Qt event loop:
//   Daemon (global hotkey + permission checks)  --hotkeyTriggered-->
//   SearchSession (capture -> OCR -> overlay pipeline) which drives
//   IScreenCapture (screenshots), OcrEngine (Tesseract), SearchOverlay (UI)
//   and CaptureFlashOverlay (visual feedback).
// Also provides: single-instance enforcement over a local socket, POSIX
// signal handling that wipes sensitive data before exit, and the menu-bar
// (tray) icon with a Quit entry.
//
// Process model: one primary instance per user session. A second launch
// connects to the first one's socket, asks it to activate and exits.
//
// Security notes: SecurityGuard::runStartupChecks() runs before anything else;
// session->dismiss() securely wipes the OCR corpus (see SecureWipe.h) and is
// called on every exit path (signal, tray Quit, normal exit).
//
// Known limits: the permission dialog text is macOS-specific (Screen
// Recording / Accessibility) and is shown only if the Daemon reports a
// missing permission; the activation policy call is guarded by Q_OS_MACOS.
// ==============================================================================

// POSIX signal numbers and sigaction (used by the secure-shutdown handler below).
#include <csignal>
// pipe/read/write/close come from <unistd.h> on POSIX. On Windows the
// equivalent underscore-prefixed CRT functions are mapped onto the same names.
#ifndef Q_OS_WIN
#include <unistd.h>   // pipe(), write(), read()
#else
#include <io.h>
#define close _close
#define read _read
#define write _write
#endif

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QSocketNotifier>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>

#include "core/Daemon.h"
#include "core/Permissions.h"
#include "core/ScreenCapture.h"
#include "core/SecurityGuard.h"
#include "core/PlatformHotkey.h"
#include "ocr/OcrEngine.h"
#include "SearchSession.h"
#include "ui/SearchOverlay.h"
#include "ui/CaptureFlashOverlay.h"

// Defined in an Objective-C++ file (see core/*.mm); makes the app an
// "accessory" (menu-bar only, no Dock icon).
#if defined(Q_OS_MACOS)
extern "C" void igi_set_activation_policy_accessory();
#endif

// Self-pipe trick: a signal handler may only call async-signal-safe functions,
// so it writes one byte into this pipe and the Qt event loop (via a
// QSocketNotifier, below) does the real cleanup on the main thread.
#ifndef Q_OS_WIN
static int g_sigPipe[2] = {-1, -1};

/** Async-signal-safe handler: only does a write() to the self-pipe. */
static void posixSignalHandler(int /*sig*/) {
    char byte = 1;
    // The result is ignored on purpose; if the pipe is full a wake-up is already pending.
    (void)::write(g_sigPipe[1], &byte, sizeof(byte));
}

/**
 * Creates the self-pipe and installs the handler for SIGTERM, SIGINT and SIGQUIT.
 * @return true on success; false if pipe() failed (then no handler is installed).
 */
static bool setupSignalHandler() {
    if (::pipe(g_sigPipe) != 0) {
        qWarning() << "[igi] Failed to create signal pipe.";
        return false;
    }
    struct sigaction sa;
    sa.sa_handler = posixSignalHandler;
    sigemptyset(&sa.sa_mask);
    // SA_RESTART: interrupted system calls resume instead of failing with EINTR.
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    return true;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────

/**
 * Program entry point.
 * @return 0 on normal exit (and when handing off to an already-running
 *         instance), 1 if Tesseract failed to initialise, otherwise the Qt
 *         event loop's exit code.
 */
int main(int argc, char* argv[]) {
    // First: refuse to run in an unsafe environment (library injection via
    // DYLD_INSERT_LIBRARIES, missing Hardened Runtime, unsigned Tesseract
    // library; see SecurityGuard.h). This must precede any work with user data.
    igi::core::SecurityGuard::runStartupChecks();

    // Name of the local (Unix/named-pipe) socket that identifies the primary instance.
    const QString socketName = QStringLiteral("igi_ocr_search_single_instance_socket");

    // ── Single Instance Check ──
    // Fast path: Try to connect to the socket WITHOUT creating QApplication.
    // This prevents the application from registering with the Wayland/X11
    // window manager (which causes taskbar flashing when the app exits).
    {
        // A QCoreApplication is enough for the socket client and, unlike
        // QApplication, does not register the process with the window system.
        // argc is copied because the constructor may modify it.
        int argc_copy = argc;
        QCoreApplication coreApp(argc_copy, argv);
        QLocalSocket clientSocket;
        clientSocket.connectToServer(socketName);
        // 300 ms: a live primary answers immediately; a missing one fails fast so
        // start-up is not delayed.
        if (clientSocket.waitForConnected(300)) {
            qDebug() << "[igi] Another instance is running — sending activation signal...";
            // On Wayland a focus-stealing activation needs an XDG activation token;
            // forward it so the primary instance can raise its overlay.
            QByteArray token = qgetenv("XDG_ACTIVATION_TOKEN");
            if (!token.isEmpty()) {
                clientSocket.write("ACTIVATE:" + token + "\n");
            } else {
                clientSocket.write("ACTIVATE\n");
            }
            clientSocket.flush();
            // Make sure the message is actually sent before this process exits.
            clientSocket.waitForBytesWritten(300);
            clientSocket.disconnectFromServer();
            return 0;
        }
    }

    // No other instance: become the primary one. The temporary QCoreApplication
    // above has been destroyed by leaving its scope, which Qt requires before
    // creating the QApplication.
    qDebug() << "[igi] Starting primary instance…";
    QApplication app(argc, argv);

// macOS: hide the Dock icon; the app lives in the menu bar.
#if defined(Q_OS_MACOS)
    igi_set_activation_policy_accessory();
#endif

// No POSIX signals on Windows, so sigOk is simply false there.
#ifndef Q_OS_WIN
    const bool sigOk = setupSignalHandler();
#else
    const bool sigOk = false;
#endif

    // Tesseract is initialised once at start-up (default data path, English) so
    // the first search does not pay the model-load cost.
    // ── OCR engine ──
    auto ocrEngine = std::make_unique<igi::ocr::OcrEngine>();
    if (!ocrEngine->init(nullptr, "eng")) {
        qWarning() << "[igi] Tesseract initialisation failed.";
        return 1;
    }
    qDebug() << "[igi] Tesseract pre-loaded.";

    // Factory picks the platform implementation (CGWindowListCreateImage on
    // macOS, Qt screen grab elsewhere).
    // ── Screen capture ──
    auto capture = igi::core::IScreenCapture::create();

    // UI objects have no parent: the overlays are top-level windows that live for
    // the whole process and are reclaimed at exit.
    // ── Search overlay ──
    auto* overlay = new igi::ui::SearchOverlay;
    auto* flashOverlay = new igi::ui::CaptureFlashOverlay;

    // The session owns the pipeline: capture -> OCR -> fuzzy search -> overlay.
    // It borrows the other objects, which therefore must outlive it (they do:
    // they are alive until main returns).
    // ── Pipeline controller ──
    auto* session = new igi::SearchSession(
        capture.get(), ocrEngine.get(), overlay, flashOverlay);

    // Single-instance IPC server. removeServer clears a stale socket file left
    // by a crashed instance, otherwise listen() would fail.
    // ── Setup Single Instance IPC Server ──
    QLocalServer localServer;
    QLocalServer::removeServer(socketName); // clean up any stale socket file
    if (localServer.listen(socketName)) {
        QObject::connect(&localServer, &QLocalServer::newConnection, [&localServer, session]() {
            while (localServer.hasPendingConnections()) {
                QLocalSocket* client = localServer.nextPendingConnection();
                if (client) {
                    // Protocol: one line "ACTIVATE" or "ACTIVATE:<xdg token>"; anything else is ignored.
                    QObject::connect(client, &QLocalSocket::readyRead, [client, session]() {
                        QByteArray data = client->readAll();
                        if (data.startsWith("ACTIVATE")) {
                            qDebug() << "[igi] Activation signal received via IPC.";
                            // Split only the first part after ':' (tokens contain no colon).
                            QList<QByteArray> parts = data.trimmed().split(':');
                            if (parts.size() > 1 && !parts[1].isEmpty()) {
                                qputenv("XDG_ACTIVATION_TOKEN", parts[1]);
                                qDebug() << "[igi] Set XDG_ACTIVATION_TOKEN=" << parts[1];
                            }
                            session->activate();
                        }
                        // The client is closed after one message; deleteLater is required because we
                        // are still inside its readyRead handler.
                        client->deleteLater();
                    });
                }
            }
        });
    }

    // Connect signal pipe to secure dismiss. QSocketNotifier fires on the main
    // thread (Qt event loop) when a byte arrives in the pipe — this is where we can
    // safely call Qt and C++ code.
    // ── Connect signal pipe to secure dismiss ──────────────────────────────
    // QSocketNotifier fires on the main thread (Qt event loop) when a byte
    // arrives in the pipe — this is where we can safely call Qt and C++ code.
#ifndef Q_OS_WIN
    if (sigOk) {
        auto* sigNotifier = new QSocketNotifier(g_sigPipe[0],
                                                 QSocketNotifier::Read, &app);
        QObject::connect(sigNotifier, &QSocketNotifier::activated,
            [&app, session](QSocketDescriptor, QSocketNotifier::Type) {
                qDebug() << "[igi] Signal received — running secure dismiss "
                            "before exit.";
                // Drain the pipe byte.
                // Drain the pipe byte.
                char byte = 0;
                (void)::read(g_sigPipe[0], &byte, sizeof(byte));
                // Wipe the OCR corpus from memory before leaving (explicit_bzero + munlock).
                // Securely wipe corpus (explicit_bzero + munlock).
                session->dismiss();
                // Cleanly exit the Qt event loop.
                app.quit();
            });
        qDebug() << "[igi] Signal handler installed "
                    "(SIGTERM/SIGINT/SIGQUIT → secure dismiss).";
    }
#endif

    // System tray (menu bar) icon with a small menu: title, hotkey hint, Quit.
    // ── System Tray Icon (Menu Bar) ──
    QSystemTrayIcon trayIcon;
    // Load the icon from macOS app bundle structure or from resources on other platforms.
    // Load the icon from macOS app bundle structure or from resources on other platforms.
#if defined(Q_OS_MACOS)
    QIcon icon(QCoreApplication::applicationDirPath() + "/../Resources/AppIcon.icns");
#else
    QIcon icon(QStringLiteral(":/resources/AppIcon.png"));
#endif
    trayIcon.setIcon(icon);
    
    QMenu trayMenu;
    QAction* titleAction = trayMenu.addAction("Igi OCR Search");
    titleAction->setEnabled(false);
    
    // Disabled actions are used as plain labels.
    QAction* hotkeyAction = trayMenu.addAction("Hotkey: " + igi::core::kPlatformHotkeyLabel);
    hotkeyAction->setEnabled(false);
    
    trayMenu.addSeparator();
    
    QAction* quitAction = trayMenu.addAction("Quit Igi");
    // Quit must wipe sensitive data first, then end the event loop.
    QObject::connect(quitAction, &QAction::triggered, &app, [session, &app]() {
        session->dismiss();
        app.quit();
    });
    
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // Daemon (hotkey + permissions) owns the global hotkey listener and the
    // permission preflight; it lives on the stack so its destructor releases the
    // hotkey before main returns.
    // ── Daemon (hotkey + permissions) ──
    igi::core::Daemon daemon;

    // Surface missing permissions to the user.
    // Surface missing permissions to the user.
    QObject::connect(&daemon, &igi::core::Daemon::permissionsChecked,
        [](bool screenGranted, bool axGranted) {
            if (screenGranted && axGranted) {
                return;
            }
            // The dialog is heap allocated with WA_DeleteOnClose so it may outlive this
            // lambda; it stays on top because the app has no main window to parent it to.
            auto* box = new QMessageBox(
                QMessageBox::Warning,
                QStringLiteral("Igi — Permissions Required"),
                QStringLiteral(
                    "Igi needs two macOS permissions to work:\n\n"
                    "  • Screen Recording — to capture the active window\n"
                    "  • Accessibility — to register the global hotkey\n\n"
                    "Click the button for the missing permission to open "
                    "System Settings."),
                QMessageBox::NoButton);

            if (!screenGranted) {
                QPushButton* btn = box->addButton(
                    QStringLiteral("Open Screen Recording Settings"),
                    QMessageBox::ActionRole);
                QObject::connect(btn, &QPushButton::clicked,
                    [] { igi::core::openScreenRecordingSettings(); });
            }
            if (!axGranted) {
                QPushButton* btn = box->addButton(
                    QStringLiteral("Open Accessibility Settings"),
                    QMessageBox::ActionRole);
                QObject::connect(btn, &QPushButton::clicked,
                    [] { igi::core::openAccessibilitySettings(); });
            }
            // Only the buttons for missing permissions are added above, plus Close.
            QPushButton* closeBtn = box->addButton(QMessageBox::Close);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowFlags(box->windowFlags() | Qt::WindowStaysOnTopHint);
            
            box->show();
            
            // Just close the dialog, do NOT quit the app!
            // Just close the dialog, do NOT quit the app!
            QObject::connect(closeBtn, &QPushButton::clicked, box, &QMessageBox::close);
        });

    // THE PIPELINE WIRE: the hotkey is the only trigger of the whole pipeline.
    // ── THE PIPELINE WIRE ──
    QObject::connect(&daemon, &igi::core::Daemon::hotkeyTriggered,
        session, &igi::SearchSession::activate);

    // Start the daemon: it checks permissions and registers the global hotkey;
    // missing permissions are reported through permissionsChecked above.
    daemon.start();
    qDebug() << "[igi] Daemon started. Listening for" << igi::core::kPlatformHotkeyLog << "…";

    // Blocks here until quit() is called.
    int ret = app.exec();

    // Ensure a final secure dismiss on clean exit (user closed the app normally).
    // Ensure a final secure dismiss on clean exit (user closed the app normally).
    session->dismiss();
    daemon.stop();

// Close both ends of the self-pipe (only if it was created).
#ifndef Q_OS_WIN
    if (g_sigPipe[0] != -1) ::close(g_sigPipe[0]);
    if (g_sigPipe[1] != -1) ::close(g_sigPipe[1]);
#endif

    return ret;
}
