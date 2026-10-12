#include <QString>
#include <QDebug>
#include <iostream>
#include "search/FuzzySearch.h"
#include "search/BoundingBox.h"

int main() {
    std::vector<igi::search::WordBox> corpus;
    igi::search::WordBox wb;
    wb.text = "revision";
    wb.normalizedText = "revision";
    corpus.push_back(wb);

    auto test = [&](QString query) {
        auto matches = igi::search::FuzzySearch::topK(query, corpus, 10, 0.70f);
        std::cout << "Query: " << query.toStdString() << " -> Matches: " << matches.size() << std::endl;
    };

    test("r");
    test("re");
    test("rev");
    test("revi");
    test("revis");
    return 0;
}
