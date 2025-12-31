/// @file test_word_frequency_analyzer.cpp
/// @brief Unit tests for WordFrequencyAnalyzer (OpenSpec #00042 Task 7.17)
/// Phase 11: Updated to use analyzeText() only (no KmlDocument dependency)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <kalahari/editor/word_frequency_analyzer.h>

#include <memory>

using namespace kalahari::editor;
using Catch::Approx;

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Basic construction", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;

    SECTION("Default state") {
        REQUIRE(analyzer.totalWordCount() == 0);
        REQUIRE(analyzer.uniqueWordCount() == 0);
        REQUIRE(analyzer.frequencies().isEmpty());
    }

    SECTION("Default settings") {
        REQUIRE(analyzer.overuseThreshold() == Approx(1.5));
        REQUIRE(analyzer.repetitionDistance() == 50);
        REQUIRE(analyzer.filterStopWords());
        REQUIRE(analyzer.language() == "en");
    }
}

// =============================================================================
// Settings Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Settings", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;

    SECTION("Set overuse threshold") {
        analyzer.setOveruseThreshold(5.0);
        REQUIRE(analyzer.overuseThreshold() == Approx(5.0));
    }

    SECTION("Set repetition distance") {
        analyzer.setRepetitionDistance(100);
        REQUIRE(analyzer.repetitionDistance() == 100);
    }

    SECTION("Set filter stop words") {
        analyzer.setFilterStopWords(false);
        REQUIRE_FALSE(analyzer.filterStopWords());
    }

    SECTION("Set language") {
        analyzer.setLanguage("pl");
        REQUIRE(analyzer.language() == "pl");
    }
}

// =============================================================================
// Text Analysis Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Analyze text", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);  // Count all words for predictable tests

    SECTION("Empty text") {
        analyzer.analyzeText("");
        REQUIRE(analyzer.totalWordCount() == 0);
        REQUIRE(analyzer.uniqueWordCount() == 0);
    }

    SECTION("Single word") {
        analyzer.analyzeText("hello");
        REQUIRE(analyzer.totalWordCount() == 1);
        REQUIRE(analyzer.uniqueWordCount() == 1);
    }

    SECTION("Simple text word count") {
        analyzer.analyzeText("hello world hello");
        REQUIRE(analyzer.totalWordCount() == 3);
        REQUIRE(analyzer.uniqueWordCount() == 2);
    }

    SECTION("Words are case-insensitive") {
        analyzer.analyzeText("Hello HELLO hello");
        REQUIRE(analyzer.totalWordCount() == 3);
        REQUIRE(analyzer.uniqueWordCount() == 1);

        auto freq = analyzer.frequencyOf("hello");
        REQUIRE(freq.count == 3);
    }

    SECTION("Punctuation is ignored") {
        analyzer.analyzeText("Hello, world! How are you?");
        REQUIRE(analyzer.totalWordCount() == 5);
    }

    SECTION("Single-letter words are skipped") {
        analyzer.analyzeText("I am a test");
        // "I" and "a" are single letters, should be skipped
        REQUIRE(analyzer.totalWordCount() == 2);  // "am", "test"
    }
}

// =============================================================================
// Frequency Calculation Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Frequency calculation", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);

    SECTION("Frequency percentage is correct") {
        analyzer.analyzeText("test test test other");
        // 4 words total, "test" appears 3 times = 75%
        auto freq = analyzer.frequencyOf("test");
        REQUIRE(freq.count == 3);
        REQUIRE(freq.percentage == Approx(75.0));
    }

    SECTION("Frequencies are sorted by count descending") {
        analyzer.analyzeText("aaa aaa aaa bbb bbb ccc");
        auto freqs = analyzer.frequencies();

        REQUIRE(freqs.size() == 3);
        REQUIRE(freqs[0].word == "aaa");
        REQUIRE(freqs[0].count == 3);
        REQUIRE(freqs[1].word == "bbb");
        REQUIRE(freqs[1].count == 2);
        REQUIRE(freqs[2].word == "ccc");
        REQUIRE(freqs[2].count == 1);
    }

    SECTION("Top N words") {
        analyzer.analyzeText("aaa aaa aaa bbb bbb ccc ddd eee");
        auto top2 = analyzer.topWords(2);

        REQUIRE(top2.size() == 2);
        REQUIRE(top2[0].word == "aaa");
        REQUIRE(top2[1].word == "bbb");
    }
}

// =============================================================================
// Overuse Detection Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Overuse detection", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);
    analyzer.setOveruseThreshold(20.0);  // 20% threshold

    SECTION("Word above threshold is marked overused") {
        // "test" appears 3 times out of 10 = 30% > 20%
        analyzer.analyzeText("test test test one two three four five six seven");

        auto overused = analyzer.overusedWords();
        REQUIRE(overused.size() == 1);
        REQUIRE(overused[0].word == "test");
        REQUIRE(overused[0].isOverused);
    }

    SECTION("Word below threshold is not marked overused") {
        // Each word appears once out of 10 = 10% < 20%
        analyzer.analyzeText("one two three four five six seven eight nine ten");

        auto overused = analyzer.overusedWords();
        REQUIRE(overused.isEmpty());
    }
}

// =============================================================================
// Stop Word Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Stop word filtering", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;

    SECTION("English stop words are filtered by default") {
        analyzer.setLanguage("en");
        analyzer.setFilterStopWords(true);

        // "the", "is", "a" are English stop words
        analyzer.analyzeText("the quick brown fox is a test");

        // Should only count: "quick", "brown", "fox", "test"
        REQUIRE(analyzer.totalWordCount() == 4);
    }

    SECTION("Stop words are counted when filtering disabled") {
        analyzer.setFilterStopWords(false);

        analyzer.analyzeText("the quick brown fox is a test");
        // 6 words counted - "a" is skipped as single letter
        REQUIRE(analyzer.totalWordCount() == 6);
    }

    SECTION("isStopWord checks correctly") {
        analyzer.setLanguage("en");

        REQUIRE(analyzer.isStopWord("the"));
        REQUIRE(analyzer.isStopWord("THE"));  // Case insensitive
        REQUIRE(analyzer.isStopWord("is"));
        REQUIRE_FALSE(analyzer.isStopWord("fox"));
        REQUIRE_FALSE(analyzer.isStopWord("test"));
    }

    SECTION("Polish stop words") {
        analyzer.setLanguage("pl");

        REQUIRE(analyzer.isStopWord("i"));
        REQUIRE(analyzer.isStopWord("jest"));
        REQUIRE(analyzer.isStopWord("nie"));
        REQUIRE_FALSE(analyzer.isStopWord("kot"));
    }
}

// =============================================================================
// Close Repetition Detection Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Close repetition detection", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);
    analyzer.setRepetitionDistance(5);  // Words within 5 positions

    SECTION("Detects close repetitions") {
        // "test" appears at positions 0 and 3, distance = 3 < 5
        analyzer.analyzeText("test one two test");

        auto reps = analyzer.closeRepetitions();
        REQUIRE(reps.size() == 1);
        REQUIRE(reps[0].word == "test");
        REQUIRE(reps[0].distance == 3);
    }

    SECTION("Ignores distant repetitions") {
        // "test" appears at positions 0 and 6, distance = 6 > 5
        analyzer.analyzeText("test one two three four five test");

        auto reps = analyzer.closeRepetitions();
        REQUIRE(reps.isEmpty());
    }

    SECTION("Multiple close repetitions") {
        // "word" at positions 0, 2, 4: repetitions (0,2) and (2,4)
        // "xx" at positions 1, 3: repetition (1,3)
        // Total: 3 repetitions
        analyzer.analyzeText("word xx word xx word");

        auto reps = analyzer.closeRepetitions();
        REQUIRE(reps.size() == 3);
    }
}

// =============================================================================
// Word Position Tests
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Word positions", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);

    SECTION("Positions are tracked correctly") {
        analyzer.analyzeText("hello world hello test hello");

        auto positions = analyzer.positionsOf("hello");
        REQUIRE(positions.size() == 3);
        REQUIRE(positions[0] == 0);
        REQUIRE(positions[1] == 2);
        REQUIRE(positions[2] == 4);
    }

    SECTION("Unknown word returns empty positions") {
        analyzer.analyzeText("hello world");

        auto positions = analyzer.positionsOf("unknown");
        REQUIRE(positions.isEmpty());
    }
}

// =============================================================================
// Signal Tests (manual verification without QSignalSpy)
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Signals", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);

    // Manual signal tracking
    int completeCount = 0;
    int lastProgress = -1;

    QObject::connect(&analyzer, &WordFrequencyAnalyzer::analysisComplete,
        [&]() { ++completeCount; });

    QObject::connect(&analyzer, &WordFrequencyAnalyzer::analysisProgress,
        [&](int percent) { lastProgress = percent; });

    SECTION("Emits analysisComplete when done") {
        analyzer.analyzeText("hello world test");

        REQUIRE(completeCount == 1);
    }

    SECTION("Emits progress signals") {
        // Generate longer text for progress updates
        QString longText;
        for (int i = 0; i < 100; ++i) {
            longText += QString("word%1 ").arg(i);
        }

        analyzer.analyzeText(longText);

        // Last progress should be 100
        REQUIRE(lastProgress == 100);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("WordFrequencyAnalyzer: Edge cases", "[editor][frequency]") {
    WordFrequencyAnalyzer analyzer;
    analyzer.setFilterStopWords(false);

    SECTION("topWords with n=0 returns empty") {
        analyzer.analyzeText("hello world");
        auto top = analyzer.topWords(0);
        REQUIRE(top.isEmpty());
    }

    SECTION("topWords with n > total returns all") {
        analyzer.analyzeText("hello world");
        auto top = analyzer.topWords(100);
        REQUIRE(top.size() == 2);
    }

    SECTION("frequencyOf unknown word returns empty struct") {
        analyzer.analyzeText("hello world");
        auto freq = analyzer.frequencyOf("unknown");
        REQUIRE(freq.word == "unknown");
        REQUIRE(freq.count == 0);
        REQUIRE(freq.percentage == Approx(0.0));
    }

    SECTION("Unicode words are handled") {
        analyzer.analyzeText("cafe naive resume");
        REQUIRE(analyzer.totalWordCount() == 3);
    }

    SECTION("Polish words are handled") {
        analyzer.analyzeText("zazolc gesla jazn");
        REQUIRE(analyzer.totalWordCount() == 3);
    }
}
