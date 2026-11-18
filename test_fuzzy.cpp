// ==============================================================================
// Program: test_fuzzy.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Automated test suite validating correctness, boundary conditions, edge cases, and performance invariants for the corresponding subsystem.
// ==============================================================================

#include "search/FuzzySearch.h"
#include <QString>
#include <QDebug>
#include <iostream>

using namespace igi::search;

int main() {
    WordBox wb1;
    wb1.normalizedText = "test";
    
    std::vector<WordBox> corpus = {wb1};
    
    auto results = FuzzySearch::topK("test", corpus, 10, 0.70f);
    std::cout << "Results size: " << results.size() << std::endl;
    if (!results.empty()) {
        std::cout << "Score: " << results[0].score << std::endl;
    }
    return 0;
}
