/// @file test_document_generator.cpp
/// @brief Implementation of test document generator for benchmarks
///
/// OpenSpec #00043 Phase 10 - Performance Benchmark Infrastructure

#include "test_document_generator.h"

#include <QFile>
#include <QTextStream>

namespace kalahari::benchmarks {

// =============================================================================
// Static Word List (~500 common English words)
// =============================================================================

const QStringList TestDocumentGenerator::s_wordList = {
    // Common words
    "the", "be", "to", "of", "and", "a", "in", "that", "have", "I",
    "it", "for", "not", "on", "with", "he", "as", "you", "do", "at",
    "this", "but", "his", "by", "from", "they", "we", "say", "her", "she",
    "or", "an", "will", "my", "one", "all", "would", "there", "their", "what",
    "so", "up", "out", "if", "about", "who", "get", "which", "go", "me",

    // Verbs
    "make", "can", "like", "time", "no", "just", "him", "know", "take", "people",
    "into", "year", "your", "good", "some", "could", "them", "see", "other", "than",
    "then", "now", "look", "only", "come", "its", "over", "think", "also", "back",
    "after", "use", "two", "how", "our", "work", "first", "well", "way", "even",
    "new", "want", "because", "any", "these", "give", "day", "most", "us", "feel",

    // Nouns
    "world", "life", "hand", "part", "child", "eye", "woman", "place", "case", "week",
    "company", "system", "program", "question", "government", "number", "night", "point", "home", "water",
    "room", "mother", "area", "money", "story", "fact", "month", "lot", "right", "study",
    "book", "word", "business", "issue", "side", "kind", "head", "house", "service", "friend",
    "father", "power", "hour", "game", "line", "end", "member", "law", "car", "city",

    // Adjectives
    "old", "great", "high", "small", "large", "next", "young", "important", "few", "public",
    "same", "able", "every", "last", "long", "own", "big", "little", "different", "political",
    "possible", "free", "human", "national", "best", "sure", "real", "certain", "early", "major",
    "local", "social", "white", "special", "open", "whole", "full", "clear", "true", "past",
    "hard", "late", "general", "strong", "private", "simple", "personal", "main", "recent", "single",

    // More common words
    "still", "find", "being", "here", "many", "through", "long", "very", "must", "might",
    "such", "since", "against", "right", "three", "before", "down", "should", "need", "both",
    "between", "each", "always", "under", "while", "another", "those", "never", "around", "during",
    "off", "without", "place", "once", "often", "though", "until", "left", "already", "done",
    "however", "almost", "where", "group", "seem", "away", "something", "problem", "perhaps", "moment",

    // Literary words for variety
    "darkness", "silence", "shadow", "whisper", "wonder", "dream", "memory", "heart", "soul", "spirit",
    "thought", "voice", "light", "truth", "hope", "fear", "love", "pain", "joy", "peace",
    "journey", "destiny", "courage", "wisdom", "beauty", "strength", "mystery", "adventure", "legend", "fate",
    "chapter", "story", "tale", "narrative", "character", "scene", "dialogue", "plot", "theme", "setting",
    "conflict", "resolution", "climax", "beginning", "ending", "passage", "paragraph", "sentence", "word", "page",

    // Action verbs
    "walk", "run", "jump", "speak", "listen", "watch", "read", "write", "sing", "dance",
    "laugh", "cry", "smile", "frown", "nod", "shake", "turn", "move", "stop", "start",
    "open", "close", "push", "pull", "hold", "drop", "catch", "throw", "break", "build",
    "create", "destroy", "change", "stay", "leave", "return", "begin", "finish", "continue", "wait",
    "search", "discover", "reveal", "hide", "show", "tell", "ask", "answer", "decide", "choose",

    // Descriptive words
    "beautiful", "wonderful", "amazing", "incredible", "fantastic", "terrible", "horrible", "excellent", "perfect", "strange",
    "quiet", "loud", "soft", "hard", "warm", "cold", "hot", "cool", "bright", "dark",
    "fast", "slow", "quick", "careful", "gentle", "rough", "smooth", "sharp", "dull", "deep",
    "shallow", "wide", "narrow", "thick", "thin", "heavy", "empty", "solid", "liquid", "ancient",
    "modern", "traditional", "contemporary", "classic", "unique", "common", "rare", "familiar", "unknown", "mysterious",

    // Time words
    "today", "tomorrow", "yesterday", "morning", "evening", "afternoon", "midnight", "dawn", "dusk", "forever",
    "always", "never", "sometimes", "often", "rarely", "usually", "frequently", "occasionally", "suddenly", "gradually",
    "immediately", "eventually", "finally", "meanwhile", "afterwards", "previously", "currently", "recently", "soon", "later",

    // Place words
    "here", "there", "everywhere", "nowhere", "somewhere", "inside", "outside", "above", "below", "beside",
    "between", "behind", "ahead", "across", "through", "around", "along", "toward", "away", "near",
    "far", "close", "distant", "nearby", "remote", "central", "northern", "southern", "eastern", "western",

    // Connecting words
    "and", "but", "or", "yet", "so", "because", "although", "however", "therefore", "moreover",
    "furthermore", "meanwhile", "otherwise", "nevertheless", "consequently", "accordingly", "similarly", "likewise", "instead", "indeed",

    // More variety
    "forest", "mountain", "river", "ocean", "sky", "cloud", "star", "moon", "sun", "wind",
    "rain", "snow", "storm", "thunder", "lightning", "fire", "earth", "stone", "tree", "flower",
    "bird", "animal", "creature", "beast", "monster", "dragon", "knight", "princess", "king", "queen",
    "castle", "tower", "bridge", "road", "path", "gate", "door", "window", "wall", "floor",
    "ceiling", "stairs", "garden", "field", "village", "town", "kingdom", "realm", "empire", "world"
};

// =============================================================================
// Constructor
// =============================================================================

TestDocumentGenerator::TestDocumentGenerator()
    : m_config()
    , m_rng(m_config.seed)
{
}

TestDocumentGenerator::TestDocumentGenerator(const Config& config)
    : m_config(config)
    , m_rng(config.seed)
{
}

// =============================================================================
// Document Generation
// =============================================================================

QString TestDocumentGenerator::generatePlainText()
{
    // Reset RNG for reproducibility
    m_rng.seed(m_config.seed);
    m_lastWordCount = 0;

    QString result;
    result.reserve(m_config.targetWordCount * 7);  // ~7 chars per word average

    int currentWordCount = 0;
    int paragraphIndex = 0;

    while (currentWordCount < m_config.targetWordCount) {
        // Determine paragraph type
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double roll = dist(m_rng);

        bool isHeading = (roll < m_config.headingRatio);
        bool isShort = (!isHeading && roll < m_config.headingRatio + m_config.shortParagraphRatio);

        // Generate paragraph
        int paragraphLength = getRandomParagraphLength(isShort);

        // Don't overshoot target word count too much
        if (currentWordCount + paragraphLength > m_config.targetWordCount + 100) {
            paragraphLength = m_config.targetWordCount - currentWordCount;
            if (paragraphLength <= 0) break;
        }

        QString paragraph = generateParagraph(paragraphLength, isHeading);

        // Add paragraph separator
        if (paragraphIndex > 0) {
            result.append("\n\n");
        }

        result.append(paragraph);
        currentWordCount += paragraphLength;
        paragraphIndex++;
    }

    m_lastWordCount = currentWordCount;
    return result;
}

QString TestDocumentGenerator::generateKml()
{
    // Reset RNG for reproducibility
    m_rng.seed(m_config.seed);
    m_lastWordCount = 0;

    QString result;
    result.reserve(m_config.targetWordCount * 10);  // Extra space for KML tags

    int currentWordCount = 0;
    int paragraphIndex = 0;

    while (currentWordCount < m_config.targetWordCount) {
        // Determine paragraph type
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double roll = dist(m_rng);

        bool isHeading = (roll < m_config.headingRatio);
        bool isShort = (!isHeading && roll < m_config.headingRatio + m_config.shortParagraphRatio);
        bool isFormatted = (!isHeading && dist(m_rng) < m_config.formattedRatio);

        // Generate paragraph
        int paragraphLength = getRandomParagraphLength(isShort);

        // Don't overshoot target word count too much
        if (currentWordCount + paragraphLength > m_config.targetWordCount + 100) {
            paragraphLength = m_config.targetWordCount - currentWordCount;
            if (paragraphLength <= 0) break;
        }

        QString paragraph;
        if (isHeading) {
            paragraph = generateParagraph(paragraphLength, true);
            paragraph = QString("<h>%1</h>").arg(paragraph);
        } else if (isFormatted) {
            paragraph = generateFormattedParagraph(paragraphLength);
            paragraph = QString("<p>%1</p>").arg(paragraph);
        } else {
            paragraph = generateParagraph(paragraphLength, false);
            paragraph = QString("<p>%1</p>").arg(paragraph);
        }

        // Add paragraph separator
        if (paragraphIndex > 0) {
            result.append("\n");
        }

        result.append(paragraph);
        currentWordCount += paragraphLength;
        paragraphIndex++;
    }

    m_lastWordCount = currentWordCount;
    return result;
}

// =============================================================================
// Helper Methods
// =============================================================================

QString TestDocumentGenerator::generateParagraph(int wordCount, bool isHeading)
{
    QString result;
    result.reserve(wordCount * 7);

    for (int i = 0; i < wordCount; ++i) {
        if (i > 0) {
            result.append(' ');
        }

        QString word = getRandomWord();

        // Capitalize first word or after period
        if (i == 0) {
            word[0] = word[0].toUpper();
        }

        result.append(word);

        // Add punctuation occasionally (not for headings)
        if (!isHeading && i > 0 && i < wordCount - 1) {
            std::uniform_int_distribution<int> dist(0, 100);
            int roll = dist(m_rng);

            if (roll < 3) {
                result.append('.');
                // Capitalize next word
                if (i + 1 < wordCount) {
                    i++;
                    result.append(' ');
                    word = getRandomWord();
                    word[0] = word[0].toUpper();
                    result.append(word);
                }
            } else if (roll < 8) {
                result.append(',');
            } else if (roll < 10) {
                result.append(';');
            }
        }
    }

    // End with appropriate punctuation
    if (!result.isEmpty()) {
        QChar lastChar = result.at(result.length() - 1);
        if (lastChar != '.' && lastChar != '!' && lastChar != '?') {
            if (isHeading) {
                // Headings don't need punctuation
            } else {
                result.append('.');
            }
        }
    }

    return result;
}

QString TestDocumentGenerator::generateFormattedParagraph(int wordCount)
{
    QString result;
    result.reserve(wordCount * 10);

    int i = 0;
    while (i < wordCount) {
        if (i > 0) {
            result.append(' ');
        }

        // Decide whether to apply formatting to next few words
        std::uniform_int_distribution<int> formatDist(0, 100);
        int formatRoll = formatDist(m_rng);

        if (formatRoll < 15 && i + 2 < wordCount) {
            // Bold (15% chance)
            std::uniform_int_distribution<int> lenDist(1, 3);
            int formatLen = std::min(lenDist(m_rng), wordCount - i);

            result.append("<b>");
            for (int j = 0; j < formatLen; ++j) {
                if (j > 0) result.append(' ');
                QString word = getRandomWord();
                if (i == 0 && j == 0) word[0] = word[0].toUpper();
                result.append(word);
            }
            result.append("</b>");
            i += formatLen;
        } else if (formatRoll < 30 && i + 2 < wordCount) {
            // Italic (15% chance)
            std::uniform_int_distribution<int> lenDist(1, 4);
            int formatLen = std::min(lenDist(m_rng), wordCount - i);

            result.append("<i>");
            for (int j = 0; j < formatLen; ++j) {
                if (j > 0) result.append(' ');
                QString word = getRandomWord();
                if (i == 0 && j == 0) word[0] = word[0].toUpper();
                result.append(word);
            }
            result.append("</i>");
            i += formatLen;
        } else {
            // Plain word
            QString word = getRandomWord();
            if (i == 0) word[0] = word[0].toUpper();
            result.append(word);
            i++;
        }

        // Add punctuation occasionally
        if (i > 1 && i < wordCount) {
            std::uniform_int_distribution<int> punctDist(0, 100);
            int punctRoll = punctDist(m_rng);

            if (punctRoll < 3) {
                result.append('.');
            } else if (punctRoll < 8) {
                result.append(',');
            }
        }
    }

    // End with period
    if (!result.isEmpty()) {
        QChar lastChar = result.at(result.length() - 1);
        if (lastChar != '.' && lastChar != '!' && lastChar != '?' && lastChar != '>') {
            result.append('.');
        }
    }

    return result;
}

QString TestDocumentGenerator::getRandomWord()
{
    std::uniform_int_distribution<int> dist(0, s_wordList.size() - 1);
    return s_wordList[dist(m_rng)];
}

int TestDocumentGenerator::getRandomParagraphLength(bool isShort)
{
    if (isShort) {
        // Dialog-like short paragraph: 5-20 words
        std::uniform_int_distribution<int> dist(5, 20);
        return dist(m_rng);
    } else {
        // Normal paragraph
        std::uniform_int_distribution<int> dist(m_config.minParagraphWords, m_config.maxParagraphWords);
        return dist(m_rng);
    }
}

bool TestDocumentGenerator::saveToFile(const QString& content, const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();

    return true;
}

} // namespace kalahari::benchmarks
