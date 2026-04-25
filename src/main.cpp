#include <QApplication>
#include <QWidget>
#include <QScreen>
#include <QPixmap>
#include <QDebug>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

int main(int argc, char *argv[]) {
    // 1. Initialize the Qt Application
    QApplication app(argc, argv);

    qDebug() << "⚡ Igi Demon Engine Starting...";

    // 2. Initialize Tesseract OCR Engine
    tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
    
    // Initialize tesseract-ocr with English, without specifying tessdata path
    // We assume the TESSDATA_PREFIX environment variable is set or data is in the default location
    if (api->Init(NULL, "eng")) {
        qWarning() << "Could not initialize tesseract. Please ensure tesseract language data is installed.";
        return 1;
    }
    
    qDebug() << "Tesseract initialized successfully.";

    // 3. Phase 1 Mockup: We will eventually use Qt to grab the screen here.
    // For now, we will just open a tiny invisible widget to prove Qt is working.
    QWidget window;
    window.resize(250, 150);
    window.setWindowTitle("Igi Background Utility");
    // window.show(); // Kept hidden as Igi is a background utility

    // Cleanup Tesseract
    api->End();
    delete api;

    // We return 0 immediately instead of app.exec() just to prove the build works
    qDebug() << "Engine built and linked perfectly!";
    return 0;
}
