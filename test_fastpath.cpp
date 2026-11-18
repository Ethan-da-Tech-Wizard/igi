// Program: igi - test_fastpath.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Manual check that a QCoreApplication can be created and destroyed before a full QApplication is created.

#include <QCoreApplication>
#include <QApplication>
#include <QDebug>

int main(int argc, char* argv[]) {
    {
        int argc_copy = argc;
        QCoreApplication coreApp(argc_copy, argv);
        qDebug() << "CoreApp constructed and destroyed.";
    }
    
    QApplication app(argc, argv);
    qDebug() << "QApplication constructed.";
    return 0;
}
