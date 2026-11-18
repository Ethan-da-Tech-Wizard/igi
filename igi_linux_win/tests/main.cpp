// Program: igi - main.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Test runner entry point that creates a QApplication and then runs all GoogleTest cases.

#include <QApplication>
#include <gtest/gtest.h>

// We use QApplication (not QCoreApplication) so tests covering
// QGuiApplication::screens() and any future widget-touching code can
// run. macos-14 GitHub runners provide a virtual display, so this
// constructs cleanly. If the suite ever needs to run truly headless,
// set QT_QPA_PLATFORM=offscreen in the CI step.
// Create the Qt application, then hand control to GoogleTest.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
