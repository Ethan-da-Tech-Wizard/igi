// Program: igi - test_split.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Manual check of how splitting a short query on whitespace counts words.

#include <QString>
#include <QRegularExpression>
#include <iostream>

int main() {
    QString query = "cr";
    int estimatedWords = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    std::cout << "estimatedWords for 'cr': " << estimatedWords << std::endl;
    return 0;
}
