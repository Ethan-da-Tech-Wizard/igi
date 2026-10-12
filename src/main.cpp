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

#if defined(Q_OS_MACOS)
extern "C" void igi_set_activation_policy_accessory();
#endif

#ifndef Q_OS_WIN
static int g_sigPipe[2] = {-1, -1};

static void posixSignalHandler(int /*sig*/) {
    char byte = 1;
    (void)::write(g_sigPipe[1], &byte, sizeof(byte));
}

static bool setupSignalHandler() {
    if (::pipe(g_sigPipe) != 0) {
        qWarning() << "[igi] Failed to create signal pipe.";
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
    igi::core::SecurityGuard::runStartupChecks();

    const QString socketName = QStringLiteral("igi_ocr_search_single_instance_socket");

    // ── Single Instance Check ──
    // Fast path: Try to connect to the socket WITHOUT creating QApplication.
    // This prevents the application from registering with the Wayland/X11
    // window manager (which causes taskbar flashing when the app exits).
    {
        int argc_copy = argc;
        QCoreApplication coreApp(argc_copy, argv);
        QLocalSocket clientSocket;
        clientSocket.connectToServer(socketName);
        if (clientSocket.waitForConnected(300)) {
            qDebug() << "[igi] Another instance is running — sending activation signal...";
            QByteArray token = qgetenv("XDG_ACTIVATION_TOKEN");
            if (!token.isEmpty()) {
                clientSocket.write("ACTIVATE:" + token + "\n");
            } else {
                clientSocket.write("ACTIVATE\n");
            }
            clientSocket.flush();
            clientSocket.waitForBytesWritten(300);
            clientSocket.disconnectFromServer();
            return 0;
        }
    }

    qDebug() << "[igi] Starting primary instance…";
    QApplication app(argc, argv);

#if defined(Q_OS_MACOS)
    igi_set_activation_policy_accessory();
#endif

#ifndef Q_OS_WIN
    const bool sigOk = setupSignalHandler();
#else
    const bool sigOk = false;
#endif

    // ── OCR engine ──
    auto ocrEngine = std::make_unique<igi::ocr::OcrEngine>();
    if (!ocrEngine->init(nullptr, "eng")) {
        qWarning() << "[igi] Tesseract initialisation failed.";
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

    // ── Setup Single Instance IPC Server ──
    QLocalServer localServer;
    QLocalServer::removeServer(socketName); // clean up any stale socket file
    if (localServer.listen(socketName)) {
        QObject::connect(&localServer, &QLocalServer::newConnection, [&localServer, session]() {
            while (localServer.hasPendingConnections()) {
                QLocalSocket* client = localServer.nextPendingConnection();
                if (client) {
                    QObject::connect(client, &QLocalSocket::readyRead, [client, session]() {
                        QByteArray data = client->readAll();
                        if (data.startsWith("ACTIVATE")) {
                            qDebug() << "[igi] Activation signal received via IPC.";
                            QList<QByteArray> parts = data.trimmed().split(':');
                            if (parts.size() > 1 && !parts[1].isEmpty()) {
                                qputenv("XDG_ACTIVATION_TOKEN", parts[1]);
                                qDebug() << "[igi] Set XDG_ACTIVATION_TOKEN=" << parts[1];
                            }
                            session->activate();
                        }
                        client->deleteLater();
                    });
                }
            }
        });
    }

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
