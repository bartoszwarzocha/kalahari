/// @file test_document_generator.h
/// @brief 150k word test document generator for performance benchmarks
///
/// OpenSpec #00043 Phase 10 - Performance Benchmark Infrastructure
/// Generates realistic test documents with configurable characteristics
/// for benchmarking the new editor architecture.

#pragma once

#include <QString>
#include <QStringList>
#include <random>

namespace kalahari::benchmarks {

/// @brief Generates realistic test documents for performance benchmarks
///
/// This class creates documents with realistic paragraph structures,
/// including varying lengths, headings, dialog-like short paragraphs,
/// and formatted text with KML markup.
///
/// Usage:
/// @code
/// TestDocumentGenerator generator;
/// QString plainText = generator.generatePlainText();
/// QString kml = generator.generateKml();
/// int wordCount = generator.lastWordCount();
/// @endcode
class TestDocumentGenerator {
public:
    /// @brief Configuration for document generation
    struct Config {
        int targetWordCount = 150000;       ///< Target word count (~150k)
        int minParagraphWords = 10;         ///< Minimum words per paragraph
        int maxParagraphWords = 200;        ///< Maximum words per paragraph
        double headingRatio = 0.05;         ///< 5% headings
        double shortParagraphRatio = 0.10;  ///< 10% dialog-like short paragraphs
        double formattedRatio = 0.20;       ///< 20% text with formatting
        unsigned int seed = 42;             ///< Random seed for reproducibility
    };

    /// @brief Construct generator with default config
    TestDocumentGenerator();

    /// @brief Construct generator with custom config
    /// @param config Configuration settings
    explicit TestDocumentGenerator(const Config& config);

    /// @brief Generate plain text document with ~150k words
    /// @return Plain text document
    QString generatePlainText();

    /// @brief Generate KML document with formatting
    /// @return KML-formatted document
    QString generateKml();

    /// @brief Get actual word count of last generated document
    /// @return Word count
    int lastWordCount() const { return m_lastWordCount; }

    /// @brief Get the configuration
    const Config& config() const { return m_config; }

    /// @brief Save generated document to file
    /// @param content Document content
    /// @param path File path
    /// @return true if saved successfully
    static bool saveToFile(const QString& content, const QString& path);

private:
    /// @brief Generate a single paragraph
    /// @param wordCount Number of words
    /// @param isHeading Whether this is a heading
    /// @return Generated paragraph text
    QString generateParagraph(int wordCount, bool isHeading);

    /// @brief Generate a paragraph with KML formatting
    /// @param wordCount Number of words
    /// @return Formatted paragraph with KML tags
    QString generateFormattedParagraph(int wordCount);

    /// @brief Get a random word from the word list
    /// @return Random word
    QString getRandomWord();

    /// @brief Get random paragraph length based on config
    /// @param isShort Whether to generate short dialog-like paragraph
    /// @return Random word count
    int getRandomParagraphLength(bool isShort);

    Config m_config;               ///< Generator configuration
    std::mt19937 m_rng;            ///< Random number generator
    int m_lastWordCount = 0;       ///< Word count of last generated document

    /// @brief Static word list for lorem ipsum-style text generation
    static const QStringList s_wordList;
};

} // namespace kalahari::benchmarks
