#include <csignal>
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

#include "core/Daemon.h"
#include "core/Permissions.h"
#include "core/ScreenCapture.h"
#include "core/SecurityGuard.h"
#include "core/PlatformHotkey.h"
#include "ocr/OcrEngine.h"
#include "SearchSession.h"
#include "ui/SearchOverlay.h"
#include "ui/CaptureFlashOverlay.h"

#if defined(Q_OS_MACOS)
extern "C" void igi_set_activation_policy_accessory();
#endif

#ifndef Q_OS_WIN
// ── POSIX signal → Qt event bridge (self-pipe trick) ─────────────────────────
//
// THREAT (T-2): If Igi is killed (SIGTERM from the OS, SIGINT from Ctrl+C, or
// SIGQUIT from a force-quit script) while an active session holds mlock()'d
// PHI in corpus_, the default signal handler terminates the process immediately
// — bypassing dismiss(), skipping explicit_bzero(), and leaving zeroed-but-not-
// yet-munlock()'d pages in RAM.
//
// FIX: The self-pipe trick bridges POSIX signals (which are async-signal-unsafe
// for Qt) into Qt's event loop safely:
//   1. A raw POSIX signal handler writes one byte to a pipe fd.
//   2. A QSocketNotifier watches the read end of that pipe.
//   3. When the notifier fires (on the main thread, inside the Qt event loop),
//      it calls session->dismiss() then app.quit() — guaranteeing the corpus
//      is zeroed and munlock()'d before the process exits.
//
// This pattern is recommended by the Qt documentation for POSIX signal handling.
static int g_sigPipe[2] = {-1, -1};

static void posixSignalHandler(int /*sig*/) {
    // async-signal-safe: write() is on the POSIX safe list.
    char byte = 1;
    (void)::write(g_sigPipe[1], &byte, sizeof(byte));
}

static bool setupSignalHandler() {
    if (::pipe(g_sigPipe) != 0) {
        qWarning() << "[igi] Failed to create signal pipe. "
                      "SIGTERM/SIGINT will not trigger secure corpus wipe.";
        return false;
    }
    struct sigaction sa;
    sa.sa_handler = posixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    return true;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // ── SEC-GUARD: run all security checks before any PHI can enter scope ──
    // This runs before QApplication so that NSLog output goes to Unified Log
    // unfiltered. Hard failures abort(). Soft warnings log to MDM syslog.
    igi::core::SecurityGuard::runStartupChecks();

    QApplication app(argc, argv);

    // Suppress Dock icon — Igi runs as a background utility.
    // The definitive suppression is LSUIElement=1 in Info.plist (Chunk 7).
#if defined(Q_OS_MACOS)
    igi_set_activation_policy_accessory();
#endif

    qDebug() << "[igi] Starting…";

    // ── Signal handling: wire SIGTERM/SIGINT/SIGQUIT → dismiss() ──────────────
    // Must be set up before any session is created so the pipe is ready.
#ifndef Q_OS_WIN
    const bool sigOk = setupSignalHandler();
#else
    const bool sigOk = false;
#endif

    // ── OCR engine ──
    // Initialised once at startup and reused across all sessions.
    auto ocrEngine = std::make_unique<igi::ocr::OcrEngine>();
    if (!ocrEngine->init(nullptr, "eng")) {
        qWarning() << "[igi] Tesseract initialisation failed. "
                      "Ensure tessdata is installed (TESSDATA_PREFIX).";
        return 1;
    }
    qDebug() << "[igi] Tesseract pre-loaded.";

    // ── Screen capture ──
    auto capture = igi::core::IScreenCapture::create();

    // ── Search overlay ──
    auto* overlay = new igi::ui::SearchOverlay;
    auto* flashOverlay = new igi::ui::CaptureFlashOverlay;

    // ── Pipeline controller ──
    auto* session = new igi::SearchSession(
        capture.get(), ocrEngine.get(), overlay, flashOverlay);

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
                char byte = 0;
                (void)::read(g_sigPipe[0], &byte, sizeof(byte));
                // Securely wipe corpus (explicit_bzero + munlock).
                session->dismiss();
                // Cleanly exit the Qt event loop.
                app.quit();
            });
        qDebug() << "[igi] Signal handler installed "
                    "(SIGTERM/SIGINT/SIGQUIT → secure dismiss).";
    }
#endif

    // ── System Tray Icon (Menu Bar) ──
    QSystemTrayIcon trayIcon;
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
    
    QAction* hotkeyAction = trayMenu.addAction("Hotkey: " + igi::core::kPlatformHotkeyLabel);
    hotkeyAction->setEnabled(false);
    
    trayMenu.addSeparator();
    
    QAction* quitAction = trayMenu.addAction("Quit Igi");
    QObject::connect(quitAction, &QAction::triggered, &app, [session, &app]() {
        session->dismiss();
        app.quit();
    });
    
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // ── Daemon (hotkey + permissions) ──
    igi::core::Daemon daemon;

    // Surface missing permissions to the user.
    QObject::connect(&daemon, &igi::core::Daemon::permissionsChecked,
        [](bool screenGranted, bool axGranted) {
            if (screenGranted && axGranted) {
                return;
            }
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
            QPushButton* closeBtn = box->addButton(QMessageBox::Close);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowFlags(box->windowFlags() | Qt::WindowStaysOnTopHint);
            
            box->show();
            
            // Just close the dialog, do NOT quit the app!
            QObject::connect(closeBtn, &QPushButton::clicked, box, &QMessageBox::close);
        });

    // ── THE PIPELINE WIRE ──
    QObject::connect(&daemon, &igi::core::Daemon::hotkeyTriggered,
        session, &igi::SearchSession::activate);

    daemon.start();
    qDebug() << "[igi] Daemon started. Listening for" << igi::core::kPlatformHotkeyLog << "…";

    int ret = app.exec();

    // Ensure a final secure dismiss on clean exit (user closed the app normally).
    session->dismiss();
    daemon.stop();

#ifndef Q_OS_WIN
    if (g_sigPipe[0] != -1) ::close(g_sigPipe[0]);
    if (g_sigPipe[1] != -1) ::close(g_sigPipe[1]);
#endif

    return ret;
}
