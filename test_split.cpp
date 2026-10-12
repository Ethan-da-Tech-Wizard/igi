#include <QString>
#include <QRegularExpression>
#include <iostream>

int main() {
    QString query = "cr";
    int estimatedWords = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    std::cout << "estimatedWords for 'cr': " << estimatedWords << std::endl;
    return 0;
}
