#include <QCoreApplication>
#include <QDebug>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "core/Daemon.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    qDebug() << "[igi] Daemon engine starting...";

    tesseract::TessBaseAPI ocr;
    if (ocr.Init(nullptr, "eng") != 0) {
        qWarning() << "[igi] Could not initialize Tesseract. "
                      "Ensure tessdata is installed (TESSDATA_PREFIX).";
        return 1;
    }
    qDebug() << "[igi] Tesseract initialized.";

    igi::core::Daemon daemon;
    daemon.start();
    qDebug() << "[igi] Daemon version:" << daemon.version()
             << "running:" << daemon.isRunning();

    // Chunk 0 milestone: prove the build pipeline links and the
    // Q_OBJECT/AUTOMOC path works. Real event loop wiring lands in Chunk 1.
    daemon.stop();
    ocr.End();

    qDebug() << "[igi] Engine built and linked successfully.";
    return 0;
}
