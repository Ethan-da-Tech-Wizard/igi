#include <QString>
#include <QDebug>
#include <iostream>
#include "search/FuzzySearch.h"
#include "search/BoundingBox.h"

int main() {
    std::vector<igi::search::WordBox> corpus;
    corpus.emplace_back("Crashon", "crashon", 0, 0, 10, 10, 99.0f, 0);

    auto matches = igi::search::FuzzySearch::topK("cr", corpus, 10, 0.70f);
    std::cout << "Matches for 'cr': " << matches.size() << std::endl;
    for (const auto& m : matches) {
        std::cout << m.word.text.toStdString() << " score: " << m.score << std::endl;
    }
    
    matches = igi::search::FuzzySearch::topK("cra", corpus, 10, 0.70f);
    std::cout << "Matches for 'cra': " << matches.size() << std::endl;
    for (const auto& m : matches) {
        std::cout << m.word.text.toStdString() << " score: " << m.score << std::endl;
    }
    return 0;
}
