#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "core/Daemon.h"
#include "core/Permissions.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Suppress Dock icon — Igi runs as a background utility.
    // The definitive suppression is LSUIElement=1 in Info.plist (Chunk 7).
    // This call handles the case where the binary is run directly without
    // an app bundle.
#if defined(Q_OS_MACOS)
    extern void igi_set_activation_policy_accessory();
    igi_set_activation_policy_accessory();
#endif

    qDebug() << "[igi] Daemon engine starting...";

    // Pre-load Tesseract. TESSDATA_PREFIX must point to the tessdata directory
    // (set automatically when installed via Homebrew; bundled in the app for
    // distribution — Chunk 7).
    tesseract::TessBaseAPI ocr;
    if (ocr.Init(nullptr, "eng") != 0) {
        qWarning() << "[igi] Tesseract initialisation failed. "
                      "Ensure tessdata is installed (TESSDATA_PREFIX).";
        return 1;
    }
    qDebug() << "[igi] Tesseract OCR engine pre-loaded.";

    igi::core::Daemon daemon;

    // Surface missing-permissions to the user with a non-blocking dialog.
    QObject::connect(&daemon, &igi::core::Daemon::permissionsMissing,
        [](bool screenOk, bool axOk) {
            QMessageBox* box = new QMessageBox(QMessageBox::Warning,
                QStringLiteral("Igi — Permissions Required"),
                QStringLiteral(
                    "Igi needs two macOS permissions to work:\n\n"
                    "  • Screen Recording — to capture the active window\n"
                    "  • Accessibility — to register the global hotkey\n\n"
                    "Click the button for the missing permission to open "
                    "System Settings."),
                QMessageBox::NoButton);

            if (!screenOk) {
                QPushButton* btn = box->addButton(
                    QStringLiteral("Open Screen Recording Settings"),
                    QMessageBox::ActionRole);
                QObject::connect(btn, &QPushButton::clicked,
                    [] { igi::core::openScreenRecordingSettings(); });
            }
            if (!axOk) {
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

    // Log each hotkey trigger (later chunks attach the capture pipeline here).
    QObject::connect(&daemon, &igi::core::Daemon::hotkeyTriggered,
        [] { qDebug() << "[igi] Cmd+Shift+F triggered — capture pipeline TBD."; });

    daemon.start();
    qDebug() << "[igi] Daemon started. Listening for Cmd+Shift+F...";

    int ret = app.exec();

    daemon.stop();
    ocr.End();
    return ret;
}
