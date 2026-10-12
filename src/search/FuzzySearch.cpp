#include "search/FuzzySearch.h"

#include <algorithm>
#include <queue>
#include <math.h>
#include <QRegularExpression>

namespace igi::search {

QString FuzzySearch::normalize(const QString& text) {
    QString result;
    result.reserve(text.length());
    // Strip punctuation and lowercase
    for (int i = 0; i < text.length(); ++i) {
        QChar c = text[i];
        if (c.isLetterOrNumber()) {
            result.append(c.toLower());
        }
    }
    return result;
}

float FuzzySearch::calculateScore(const QString& queryNorm, const QString& candidateNorm, float threshold, std::vector<int>& prevRow, std::vector<int>& currRow) {
    const int len1 = queryNorm.length();
    const int len2 = candidateNorm.length();
    const int maxLen = std::max(len1, len2);

    const QChar* qData = queryNorm.constData();
    const QChar* cData = candidateNorm.constData();

    if (maxLen == 0) {
        return 1.0f;
    }

    // Maximum allowed distance to still meet the threshold
    // score = 1.0 - dist / maxLen >= threshold
    // dist <= (1.0 - threshold) * maxLen
    const int maxAllowedDist = static_cast<int>(std::floor((1.0f - threshold) * maxLen));

    // Wagner-Fischer with row-rolling
    prevRow.resize(len2 + 1);
    currRow.resize(len2 + 1);

    for (int j = 0; j <= len2; ++j) {
        prevRow[j] = j;
    }

    for (int i = 1; i <= len1; ++i) {
        currRow[0] = i;
        int rowMinDist = currRow[0];

        for (int j = 1; j <= len2; ++j) {
            int cost = (qData[i - 1] == cData[j - 1]) ? 0 : 1;
            currRow[j] = std::min({
                prevRow[j] + 1,       // deletion
                currRow[j - 1] + 1,   // insertion
                prevRow[j - 1] + cost // substitution
            });
            rowMinDist = std::min(rowMinDist, currRow[j]);
        }

        // Early exit: if the minimum distance in this row already exceeds maxAllowedDist,
        // it's mathematically impossible for the final distance to be <= maxAllowedDist.
        if (rowMinDist > maxAllowedDist) {
            return 0.0f; 
        }

        prevRow.swap(currRow);
    }

    int finalDist = prevRow[len2];
    float score = 1.0f - (static_cast<float>(finalDist) / static_cast<float>(maxLen));
    return score;
}

std::vector<Match> FuzzySearch::topK(const QString& query, const std::vector<WordBox>& corpus, int k, float threshold) {
    QString queryNorm = normalize(query);
    if (queryNorm.isEmpty() || corpus.empty() || k <= 0) {
        return {};
    }

    // Min-heap to keep track of the top-K matches
    auto cmp = [](const Match& a, const Match& b) {
        // We want the smallest score at the top of the min-heap
        return a.score > b.score;
    };
    std::priority_queue<Match, std::vector<Match>, decltype(cmp)> minHeap(cmp);

    std::vector<int> prevRow;
    std::vector<int> currRow;

    for (int i = 0; i < corpus.size(); ++i) {
        QString windowText;
        QString windowNorm;
        int minX = corpus[i].x;
        int minY = corpus[i].y;
        int maxX = corpus[i].x + corpus[i].w;
        int maxY = corpus[i].y + corpus[i].h;
        
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
            // by restricting matches to exact or prefix only.
            if (queryNorm.length() <= 2) {
                if (windowNorm == queryNorm) {
                    score = 1.0f;
                } else if (windowNorm.startsWith(queryNorm)) {
                    score = 0.90f;
                }
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
            
            if (score >= threshold) {
                WordBox mergedBox;
                mergedBox.text = windowText;
                mergedBox.normalizedText = windowNorm;
                mergedBox.x = minX;
                mergedBox.y = minY;
                mergedBox.w = maxX - minX;
                mergedBox.h = maxY - minY;

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

    // The heap pops in ascending order of score (smallest first),
    // but we want the final results in descending order (best first).
    std::reverse(results.begin(), results.end());

    return results;
}

} // namespace igi::search
