#include <QApplication>
#include <gtest/gtest.h>

// We use QApplication (not QCoreApplication) so tests covering
// QGuiApplication::screens() and any future widget-touching code can
// run. macos-14 GitHub runners provide a virtual display, so this
// constructs cleanly. If the suite ever needs to run truly headless,
// set QT_QPA_PLATFORM=offscreen in the CI step.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
