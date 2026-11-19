// ==============================================================================
// Program: FuzzySearch.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Implements high-performance approximate string matching using Wagner-Fischer dynamic programming with threshold-based early exit pruning, length heuristic filters, and top-k result ranking over OCR word boxes.
//
// How it fits: SearchSession::onQueryChanged() calls topK() on every keystroke
// with the OCR (or PDF) corpus; the returned Match list becomes the highlight
// rectangles. normalize() is also used when the corpus is built (each WordBox
// stores its normalizedText) so the same cleaning applies to both sides.
//
// Scoring in one sentence: exact match 1.0, prefix 0.90, substring 0.80,
// otherwise 1 - editDistance / max(length) (so 1.0 is identical, 0.0 is
// completely different).
//
// Performance: runs on the GUI thread for every keystroke, so it uses a
// two-row Wagner-Fischer table (O(candidate length) memory), reuses the row
// buffers across candidates and abandons a candidate as soon as it can no
// longer reach the threshold.
//
// Known limits: windows of consecutive words are built in corpus (reading)
// order, which is the OCR order, not geometric adjacency; overlapping
// windows can produce overlapping matches; comparison is per UTF-16 unit.
// ==============================================================================

#include "search/FuzzySearch.h"

#include <algorithm>
#include <queue>
#include <math.h>
#include <QRegularExpression>

namespace igi::search {

/**
 * Lower-cases the text and keeps only letters and digits (drops spaces and
 * punctuation) so "Hello, World!" becomes "helloworld". Both the query and
 * every corpus word go through this before scoring.
 * @param text Raw text.
 * @return Normalised copy.
 */
QString FuzzySearch::normalize(const QString& text) {
    QString result;
    result.reserve(text.length());
    // Keep only alphanumerics; QChar::isLetterOrNumber is Unicode aware.
    for (int i = 0; i < text.length(); ++i) {
        QChar c = text[i];
        if (c.isLetterOrNumber()) {
            result.append(c.toLower());
        }
    }
    return result;
}

/**
 * Similarity of two already-normalised strings in the range 0..1.
 *
 * score = 1 - Levenshtein(query, candidate) / max(len1, len2). Returns early
 * with 0.0 when the distance cannot stay within what @p threshold allows, so
 * a returned 0.0 means "below threshold", not necessarily "no similarity".
 *
 * @param queryNorm     Normalised query.
 * @param candidateNorm Normalised candidate text.
 * @param threshold     Minimum score the caller cares about (0..1).
 * @param prevRow       Scratch buffer, resized as needed (reused between calls to avoid allocation).
 * @param currRow       Scratch buffer, same purpose.
 * @return Score in 0..1, or 0.0 if pruned. Two empty strings score 1.0.
 */
float FuzzySearch::calculateScore(const QString& queryNorm, const QString& candidateNorm, float threshold, std::vector<int>& prevRow, std::vector<int>& currRow) {
    const int len1 = queryNorm.length();
    const int len2 = candidateNorm.length();
    const int maxLen = std::max(len1, len2);

    const QChar* qData = queryNorm.constData();
    const QChar* cData = candidateNorm.constData();

    if (maxLen == 0) {
        return 1.0f;
    }

    // Pruning bound. score >= threshold  <=>  dist <= (1 - threshold) * maxLen,
    // so any partial distance above that can never recover.
    const int maxAllowedDist = static_cast<int>(std::floor((1.0f - threshold) * maxLen));

    // Wagner-Fischer dynamic programming with two alternating rows instead of the
    // full (len1+1) x (len2+1) table: only the previous row is needed to compute
    // the next, which cuts memory from O(len1*len2) to O(len2).
    prevRow.resize(len2 + 1);
    currRow.resize(len2 + 1);

    // Row 0 = distance from the empty query prefix: j insertions.
    for (int j = 0; j <= len2; ++j) {
        prevRow[j] = j;
    }

    for (int i = 1; i <= len1; ++i) {
        // Column 0 = i deletions from the query prefix.
        currRow[0] = i;
        int rowMinDist = currRow[0];

        for (int j = 1; j <= len2; ++j) {
            int cost = (qData[i - 1] == cData[j - 1]) ? 0 : 1;
            // Standard edit-distance recurrence (equal characters cost 0, otherwise 1).
            currRow[j] = std::min({
                prevRow[j] + 1,       // deletion
                currRow[j - 1] + 1,   // insertion
                prevRow[j - 1] + cost // substitution
            });
            rowMinDist = std::min(rowMinDist, currRow[j]);
        }

        // Early exit: edit distances only grow along a row path, so if even the best
        // cell of this row exceeds maxAllowedDist the final distance must too.
        if (rowMinDist > maxAllowedDist) {
            return 0.0f; 
        }

        // Roll the rows: the current row becomes the previous one for the next query character.
        prevRow.swap(currRow);
    }

    // After the last query character prevRow holds the final row; the last cell is the distance.
    int finalDist = prevRow[len2];
    float score = 1.0f - (static_cast<float>(finalDist) / static_cast<float>(maxLen));
    return score;
}

/**
 * Finds the best @p k matches for @p query among the corpus words.
 *
 * Each corpus position starts a "window" of up to N consecutive words, where N
 * is the number of whitespace-separated words in the query, so a multi-word
 * query can match a phrase. Every window length from 1 to N is scored; a
 * match covers the union of the word rectangles in the window.
 *
 * @param query     Text typed by the user.
 * @param corpus    Words with pixel rectangles (OCR or PDF).
 * @param k         Maximum number of matches to return (<= 0 gives none).
 * @param threshold Minimum score for a match (0..1).
 * @return Matches ordered best score first; empty if the query normalises to nothing.
 */
std::vector<Match> FuzzySearch::topK(const QString& query, const std::vector<WordBox>& corpus, int k, float threshold) {
    QString queryNorm = normalize(query);
    if (queryNorm.isEmpty() || corpus.empty() || k <= 0) {
        return {};
    }

    // Keep only the K best so far in a min-heap: the weakest of them sits on top
    // and is evicted when something better arrives (O(n log k) overall).
    auto cmp = [](const Match& a, const Match& b) {
        // std::priority_queue is a max-heap by default; comparing with ">" flips it.
        return a.score > b.score;
    };
    std::priority_queue<Match, std::vector<Match>, decltype(cmp)> minHeap(cmp);

    // Scratch rows shared by every calculateScore call (avoids per-candidate allocation).
    std::vector<int> prevRow;
    std::vector<int> currRow;

    // Window start at corpus[i]; the rectangle starts as that word's box and grows per word added.
    for (int i = 0; i < corpus.size(); ++i) {
        QString windowText;
        QString windowNorm;
        int minX = corpus[i].x;
        int minY = corpus[i].y;
        int maxX = corpus[i].x + corpus[i].w;
        int maxY = corpus[i].y + corpus[i].h;
        
        // Number of words in the query decides the window size. (Computed per start
        // index although it does not depend on i.)
        int estimatedWords = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
        if (estimatedWords == 0) estimatedWords = 1;
        
        for (int w = 0; w < estimatedWords && i + w < corpus.size(); ++w) {
            const WordBox& currentWord = corpus[i + w];
            if (w > 0) {
                windowText += " ";
            }
            windowText += currentWord.text;
            windowNorm += currentWord.normalizedText;
            
            minX = std::min(minX, currentWord.x);
            minY = std::min(minY, currentWord.y);
            maxX = std::max(maxX, currentWord.x + currentWord.w);
            maxY = std::max(maxY, currentWord.y + currentWord.h);

            float score = 0.0f;

            // Prevent visual overload for very short queries (1-2 characters)
            // by restricting matches to exact or prefix only: edit distance on two
            // letters would match almost every word.
            if (queryNorm.length() <= 2) {
                if (windowNorm == queryNorm) {
                    score = 1.0f;
                } else if (windowNorm.startsWith(queryNorm)) {
                    score = 0.90f;
                }
            // Normal queries: cheap exact / prefix / substring tests first (fixed scores
            // 1.0 / 0.90 / 0.80), edit distance only as the last resort.
            } else {
                if (windowNorm == queryNorm) {
                    score = 1.0f;
                } else if (windowNorm.startsWith(queryNorm)) {
                    score = 0.90f;
                } else if (windowNorm.contains(queryNorm)) {
                    score = 0.80f;
                } else {
                    score = calculateScore(queryNorm, windowNorm, threshold, prevRow, currRow);
                }
            }
            
            // Merge the window into one WordBox whose rectangle is the union of its words.
            if (score >= threshold) {
                WordBox mergedBox;
                mergedBox.text = windowText;
                mergedBox.normalizedText = windowNorm;
                mergedBox.x = minX;
                mergedBox.y = minY;
                mergedBox.w = maxX - minX;
                mergedBox.h = maxY - minY;

                // Heap not full: take it. Otherwise replace the current worst if this is better.
                if (minHeap.size() < static_cast<size_t>(k)) {
                    minHeap.push({mergedBox, score});
                } else if (score > minHeap.top().score) {
                    minHeap.pop();
                    minHeap.push({mergedBox, score});
                }
            }
        }
    }

    std::vector<Match> results;
    results.reserve(minHeap.size());
    while (!minHeap.empty()) {
        results.push_back(minHeap.top());
        minHeap.pop();
    }

    // The heap pops worst first; reverse so the best match comes first.
    std::reverse(results.begin(), results.end());

    return results;
}

} // namespace igi::search
