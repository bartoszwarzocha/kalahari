/// @file word_frequency_analyzer.h
/// @brief Word frequency analysis and repetition detection (OpenSpec #00042 Tasks 6.18-6.20)
///
/// WordFrequencyAnalyzer provides tools for writers to improve their prose:
/// - Most frequently used words in document
/// - Overused words detection (above configurable threshold)
/// - Close repetitions detection (same word within N words)
/// - Stop words filtering (common words like "the", "a", "i", "to")
///
/// Supports multiple languages (English, Polish) with built-in stop word lists.
/// Unicode-aware word extraction using QRegularExpression.

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QSet>

namespace kalahari::editor {

/// @brief Information about word frequency
///
/// Contains statistics about a single word including its count,
/// percentage of total words, and whether it's considered overused.
struct WordFrequency {
    QString word;           ///< The word (lowercase)
    int count{0};           ///< Total occurrences
    double percentage{0.0}; ///< Percentage of total words
    bool isOverused{false}; ///< Above overuse threshold

    /// @brief Comparison operator for sorting by count (descending)
    bool operator<(const WordFrequency& other) const {
        return count > other.count;  // Sort descending by count
    }
};

/// @brief Information about a close repetition
///
/// Identifies when the same word appears multiple times within
/// a short distance, which can indicate awkward prose.
struct CloseRepetition {
    QString word;           ///< The repeated word
    int firstPos{0};        ///< Position of first occurrence (word index)
    int secondPos{0};       ///< Position of second occurrence (word index)
    int distance{0};        ///< Words between occurrences
};

/// @brief Analyzes word frequency and detects overused words
///
/// WordFrequencyAnalyzer helps writers identify:
/// - Most frequently used words
/// - Words that appear too often (above threshold percentage)
/// - Words repeated in close proximity
///
/// Example usage:
/// @code
/// auto analyzer = new WordFrequencyAnalyzer(this);
/// analyzer->setDocument(document);
/// analyzer->setOveruseThreshold(1.5);  // 1.5% of total words
/// analyzer->analyze();
///
/// auto top20 = analyzer->topWords(20);
/// auto overused = analyzer->overusedWords();
/// auto repetitions = analyzer->closeRepetitions();
/// @endcode
class WordFrequencyAnalyzer : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a word frequency analyzer
    /// @param parent Parent QObject for ownership
    explicit WordFrequencyAnalyzer(QObject* parent = nullptr);

    /// @brief Destructor
    ~WordFrequencyAnalyzer() override;

    // Non-copyable
    WordFrequencyAnalyzer(const WordFrequencyAnalyzer&) = delete;
    WordFrequencyAnalyzer& operator=(const WordFrequencyAnalyzer&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set overuse threshold (default: 1.5% of total words)
    /// @param percentage Threshold as percentage (e.g., 1.5 for 1.5%)
    void setOveruseThreshold(double percentage);

    /// @brief Get current overuse threshold
    /// @return Threshold as percentage
    double overuseThreshold() const;

    /// @brief Set close repetition distance (default: 50 words)
    /// @param words Maximum distance in words to consider as close repetition
    void setRepetitionDistance(int words);

    /// @brief Get current repetition distance
    /// @return Distance in words
    int repetitionDistance() const;

    /// @brief Enable/disable stop words filtering
    /// @param filter true to filter out stop words from analysis
    void setFilterStopWords(bool filter);

    /// @brief Check if stop words filtering is enabled
    /// @return true if filtering is enabled
    bool filterStopWords() const;

    /// @brief Set language for stop words (default: "en")
    /// @param lang Language code ("en" for English, "pl" for Polish)
    void setLanguage(const QString& lang);

    /// @brief Get current language setting
    /// @return Language code
    QString language() const;

    // =========================================================================
    // Analysis
    // =========================================================================

    /// @brief Analyze specific text
    /// @param text Text to analyze
    /// @note Emits analysisComplete() when done
    void analyzeText(const QString& text);

    /// @brief Get all word frequencies (sorted by count descending)
    /// @return List of word frequencies
    QList<WordFrequency> frequencies() const;

    /// @brief Get top N most frequent words
    /// @param n Number of top words to return (default: 20)
    /// @return List of top N word frequencies
    QList<WordFrequency> topWords(int n = 20) const;

    /// @brief Get overused words only
    /// @return List of words above overuse threshold
    QList<WordFrequency> overusedWords() const;

    /// @brief Get close repetitions
    /// @return List of close repetition instances
    QList<CloseRepetition> closeRepetitions() const;

    /// @brief Get total word count
    /// @return Total number of words analyzed
    int totalWordCount() const;

    /// @brief Get unique word count
    /// @return Number of unique words
    int uniqueWordCount() const;

    // =========================================================================
    // Word Lookup
    // =========================================================================

    /// @brief Get frequency for specific word
    /// @param word Word to look up (case-insensitive)
    /// @return Word frequency info (count=0 if not found)
    WordFrequency frequencyOf(const QString& word) const;

    /// @brief Find all positions of a word in document
    /// @param word Word to find (case-insensitive)
    /// @return List of word positions (indices)
    QList<int> positionsOf(const QString& word) const;

    /// @brief Check if word is a stop word
    /// @param word Word to check (case-insensitive)
    /// @return true if word is in current language's stop word list
    bool isStopWord(const QString& word) const;

signals:
    /// @brief Emitted when analysis is complete
    void analysisComplete();

    /// @brief Emitted with progress during analysis (0-100)
    /// @param percent Progress percentage (0-100)
    void analysisProgress(int percent);

private:
    /// @brief Build stop word lists for all supported languages
    void buildStopWordLists();

    /// @brief Extract words from text using Unicode-aware regex
    /// @param text Text to extract words from
    /// @return List of lowercase words (min length 2)
    QStringList extractWords(const QString& text) const;

    /// @brief Detect close repetitions in analyzed text
    void detectCloseRepetitions();

    // Settings
    double m_overuseThreshold{1.5};   ///< Percentage threshold for overuse
    int m_repetitionDistance{50};     ///< Words between repetitions
    bool m_filterStopWords{true};     ///< Filter stop words from analysis
    QString m_language{"en"};         ///< Language code for stop words

    // Results
    QHash<QString, int> m_wordCounts;            ///< Word -> count mapping
    QList<WordFrequency> m_frequencies;          ///< Sorted frequency list
    QList<CloseRepetition> m_repetitions;        ///< Close repetition instances
    int m_totalWords{0};                         ///< Total words analyzed

    // Word positions for repetition detection
    QHash<QString, QList<int>> m_wordPositions;  ///< Word -> list of positions

    // Stop words by language
    QHash<QString, QSet<QString>> m_stopWords;   ///< Language -> stop words set
};

}  // namespace kalahari::editor
