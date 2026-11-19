// ==============================================================================
// Program: FuzzySearch.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Implements high-performance approximate string matching using Wagner-Fischer dynamic programming with threshold-based early exit pruning, length heuristic filters, and top-k result ranking over OCR word boxes.
// ==============================================================================

#pragma once

#include <vector>
#include <QString>
#include "search/BoundingBox.h"

/** One search hit: the (possibly merged) word box and its similarity score. */
namespace igi::search {

// word.text is the display text of the window; word.x/y/w/h is the union of
// the rectangles of the words it covers (physical pixels, like all WordBox coordinates).
struct Match {
    WordBox word;
    // 1.0 = exact match, 0.90 prefix, 0.80 substring, below that edit-distance based.
    float score; // 0.0 to 1.0
};

// Namespace-level utility class: only static functions, never instantiated.
class FuzzySearch {
public:
    /**
     * Finds the best matches for a query.
     * @param query     Text typed by the user.
     * @param corpus    Words to search (from OCR or a PDF).
     * @param k         Maximum number of results.
     * @param threshold Minimum score (default 0.85; SearchSession passes 0.70).
     * @return Up to k matches, best first. Query and corpus words are lower-cased
     *         and stripped of punctuation before scoring (see normalize()).
     */
    static std::vector<Match> topK(const QString& query, const std::vector<WordBox>& corpus, int k, float threshold = 0.85f);

    // Exposed (public) so unit tests can check the edit-distance score directly;
    // see calculateScore() in FuzzySearch.cpp for the parameters.
    static float calculateScore(const QString& queryNorm, const QString& candidateNorm, float threshold, std::vector<int>& prevRow, std::vector<int>& currRow);

    // Exposed so tests can check the text cleaning.
    static QString normalize(const QString& text);
};

} // namespace igi::search
