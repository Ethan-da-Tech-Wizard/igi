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
