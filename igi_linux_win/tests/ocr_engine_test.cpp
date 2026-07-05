#include <gtest/gtest.h>

#include <QImage>
#include <QPainter>
#include <QFont>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "ocr/OcrEngine.h"

using namespace igi::ocr;
using namespace igi::search;

TEST(OcrEngine, InitSucceeds) {
    OcrEngine engine;
    // TESSDATA_PREFIX environment variable should be set, or default Homebrew path used.
    EXPECT_TRUE(engine.init());
}

TEST(OcrEngine, RecognizeExtractsWordsAndRunsAsync) {
    OcrEngine engine;
    ASSERT_TRUE(engine.init());

    QImage image(800, 400, QImage::Format_ARGB32);
    image.fill(Qt::white);
    
    QPainter painter(&image);
    QFont font(QStringLiteral("DejaVu Sans"), 36);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(QRect(0, 0, 800, 100), Qt::AlignCenter, "PATIENT REPORT");
    painter.end();
    image.save(QStringLiteral("/home/ethan/igi/test_render.png"));

    PixPtr pix = ImageConverter::qImageToPix(image);
    ASSERT_TRUE(pix != nullptr);

    QElapsedTimer timer;
    timer.start();

    // Assert that the event loop can run while OCR is running (async test)
    auto future = engine.recognizeAsync(std::move(pix));
    
    int eventsProcessed = 0;
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
        eventsProcessed++;
    }

    auto results = future.result();
    qint64 elapsedMs = timer.elapsed();

    // Log latency for CI
    std::cout << "[   INFO   ] OCR Latency: " << elapsedMs << " ms" << std::endl;
    std::cout << "[   INFO   ] Event loop iterations while blocking: " << eventsProcessed << std::endl;

    ASSERT_GE(results.size(), 2);
    
    bool foundPatient = false;
    bool foundReport = false;
    
    std::cout << "DEBUG: results size = " << results.size() << std::endl;
    for (const auto& word : results) {
        std::cout << "DEBUG WORD: '" << word.text.toStdString() << "' confidence: " << word.confidence << std::endl;
        if (word.text.contains("PATIENT", Qt::CaseInsensitive)) {
            foundPatient = true;
            EXPECT_GT(word.confidence, 60.0f);
            EXPECT_GT(word.w, 0);
            EXPECT_GT(word.h, 0);
        }
        if (word.text.contains("REPORT", Qt::CaseInsensitive)) {
            foundReport = true;
            EXPECT_GT(word.confidence, 60.0f);
        }
    }
    
    EXPECT_TRUE(foundPatient);
    EXPECT_TRUE(foundReport);
}
