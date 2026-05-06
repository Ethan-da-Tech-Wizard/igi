#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>

#include "core/Daemon.h"
#include "core/Permissions.h"
#include "core/ScreenCapture.h"
#include "ocr/OcrEngine.h"
#include "SearchSession.h"
#include "ui/SearchOverlay.h"

#if defined(Q_OS_MACOS)
extern "C" void igi_set_activation_policy_accessory();
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Suppress Dock icon — Igi runs as a background utility.
    // The definitive suppression is LSUIElement=1 in Info.plist (Chunk 7).
#if defined(Q_OS_MACOS)
    igi_set_activation_policy_accessory();
#endif

    qDebug() << "[igi] Starting…";

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

    // ── Pipeline controller ──
    auto* session = new igi::SearchSession(
        capture.get(), ocrEngine.get(), overlay);

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
            box->addButton(QMessageBox::Ignore);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setWindowFlags(box->windowFlags() | Qt::WindowStaysOnTopHint);
            box->show();
        });

    // ── THE PIPELINE WIRE ──
    // Cmd+Shift+F → capture → OCR → search ready.
    QObject::connect(&daemon, &igi::core::Daemon::hotkeyTriggered,
        session, &igi::SearchSession::activate);

    daemon.start();
    qDebug() << "[igi] Daemon started. Listening for Cmd+Shift+F…";

    int ret = app.exec();

    daemon.stop();
    return ret;
}
