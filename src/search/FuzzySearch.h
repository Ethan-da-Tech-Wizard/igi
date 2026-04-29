#pragma once

#include <vector>
#include <QString>
#include "search/BoundingBox.h"

namespace igi::search {

struct Match {
    WordBox word;
    float score; // 0.0 to 1.0
};

class FuzzySearch {
public:
    // Returns top-K matches scoring >= threshold.
    // Uses normalized Levenshtein distance: 1.0 - dist / max(len(query), len(candidate))
    // query and corpus words are lowercased and stripped of punctuation before scoring.
    static std::vector<Match> topK(const QString& query, const std::vector<WordBox>& corpus, int k, float threshold = 0.85f);

    // Visible for testing the exact distance score algorithm
    static float calculateScore(const QString& queryNorm, const QString& candidateNorm, float threshold, std::vector<int>& prevRow, std::vector<int>& currRow);

    // Visible for testing the normalization
    static QString normalize(const QString& text);
};

} // namespace igi::search
