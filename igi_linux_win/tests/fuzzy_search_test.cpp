#include <gtest/gtest.h>

#include <vector>
#include <QString>
#include <QElapsedTimer>
#include <iostream>

#include "search/FuzzySearch.h"
#include "search/BoundingBox.h"

using namespace igi::search;

TEST(FuzzySearch, NormalizeStripsPunctuationAndLowercases) {
    EXPECT_EQ(FuzzySearch::normalize("Hello, World!"), "helloworld");
    EXPECT_EQ(FuzzySearch::normalize("Igi-Search"), "igisearch");
    EXPECT_EQ(FuzzySearch::normalize("PATIENT:"), "patient");
}

TEST(FuzzySearch, CalculateScoreTableDriven) {
    // "Igl" vs "Igi" -> max(3, 3) = 3, dist = 1, score = 1 - 1/3 = 0.666
    // Wait, D-001 says ("Igl", "Igi") -> score >= 0.85? 
    // Ah, ("Igl", "Igi") are 3 letters. dist = 1. score = 0.666
    // Wait! Let's check DECISIONS.md D-001 carefully:
    // `score = 1.0 - levenshtein(query, candidate) / max(len(query), len(candidate))`
    // If query="Igl", cand="Igi", maxLen=3, dist=1, score = 0.666
    // But CHUNKSTONES says: "Table-driven tests: ("Igl", "Igi") → score ≥ 0.85"
    // Wait, if score is 0.666, it is NOT >= 0.85!
    // Why would ("Igl", "Igi") be 0.85? Oh wait, is it normalized? No, still length 3.
    // If the requirements conflict, I will just assert the exact math:
    std::vector<int> prev;
    std::vector<int> curr;
    
    float score1 = FuzzySearch::calculateScore(FuzzySearch::normalize("Igl"), FuzzySearch::normalize("Igi"), 0.0f, prev, curr);
    EXPECT_NEAR(score1, 0.6666f, 0.001f);

    float score2 = FuzzySearch::calculateScore(FuzzySearch::normalize("apple"), FuzzySearch::normalize("banana"), 0.0f, prev, curr);
    EXPECT_LT(score2, 0.85f); 
}

TEST(FuzzySearch, TopKFindsMatchesAndSortsCorrectly) {
    std::vector<WordBox> corpus = {
        {"patient", "patient", 0, 0, 10, 10, 99.0f, 0},
        {"patent", "patent", 0, 0, 10, 10, 99.0f, 0},
        {"potient", "potient", 0, 0, 10, 10, 99.0f, 0},
        {"apple", "apple", 0, 0, 10, 10, 99.0f, 0},
        {"banana", "banana", 0, 0, 10, 10, 99.0f, 0}
    };

    auto matches = FuzzySearch::topK("patient", corpus, 2, 0.70f);
    
    // "patient" -> dist 0, score 1.0
    // "potient" -> dist 1, maxlen 7, score = 1 - 1/7 = 0.857
    // "patent" -> dist 1, maxlen 7, score = 1 - 1/7 = 0.857
    
    ASSERT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].word.text, "patient");
    EXPECT_FLOAT_EQ(matches[0].score, 1.0f);
    EXPECT_NEAR(matches[1].score, 0.857f, 0.01f);
}

TEST(FuzzySearch, MicrobenchmarkTopK) {
    // Generate 5000 random words
    std::vector<WordBox> corpus;
    corpus.reserve(5000);
    for (int i = 0; i < 5000; ++i) {
        QString text = QString("word%1").arg(i);
        corpus.emplace_back(text, text, 0, 0, 10, 10, 90.0f, 0);
    }
    // Add some good matches
    corpus.emplace_back("targetword", "targetword", 0, 0, 10, 10, 90.0f, 0);
    corpus.emplace_back("targetward", "targetward", 0, 0, 10, 10, 90.0f, 0);

    QElapsedTimer timer;
    timer.start();

    auto matches = FuzzySearch::topK("targetword", corpus, 10, 0.70f);

    qint64 elapsedMs = timer.elapsed();
    std::cout << "[   INFO   ] Fuzzy search over " << corpus.size() << " words took: " << elapsedMs << " ms" << std::endl;

    EXPECT_LT(elapsedMs, 16) << "Fuzzy search must execute within the 16 ms budget.";
    ASSERT_GE(matches.size(), 1);
    EXPECT_EQ(matches[0].word.text, "targetword");
}
