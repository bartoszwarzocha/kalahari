/// @file word_frequency_analyzer.cpp
/// @brief Word frequency analysis implementation (OpenSpec #00042 Tasks 6.18-6.20)

#include <kalahari/editor/word_frequency_analyzer.h>
#include <kalahari/core/logger.h>

#include <QRegularExpression>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

WordFrequencyAnalyzer::WordFrequencyAnalyzer(QObject* parent)
    : QObject(parent)
{
    buildStopWordLists();
    core::Logger::getInstance().debug("WordFrequencyAnalyzer created");
}

WordFrequencyAnalyzer::~WordFrequencyAnalyzer()
{
    core::Logger::getInstance().debug("WordFrequencyAnalyzer destroyed");
}

// =============================================================================
// Setup
// =============================================================================

void WordFrequencyAnalyzer::setOveruseThreshold(double percentage)
{
    m_overuseThreshold = percentage;
}

double WordFrequencyAnalyzer::overuseThreshold() const
{
    return m_overuseThreshold;
}

void WordFrequencyAnalyzer::setRepetitionDistance(int words)
{
    m_repetitionDistance = words;
}

int WordFrequencyAnalyzer::repetitionDistance() const
{
    return m_repetitionDistance;
}

void WordFrequencyAnalyzer::setFilterStopWords(bool filter)
{
    m_filterStopWords = filter;
}

bool WordFrequencyAnalyzer::filterStopWords() const
{
    return m_filterStopWords;
}

void WordFrequencyAnalyzer::setLanguage(const QString& lang)
{
    m_language = lang;
}

QString WordFrequencyAnalyzer::language() const
{
    return m_language;
}

// =============================================================================
// Analysis
// =============================================================================

void WordFrequencyAnalyzer::analyzeText(const QString& text)
{
    m_wordCounts.clear();
    m_wordPositions.clear();
    m_frequencies.clear();
    m_repetitions.clear();
    m_totalWords = 0;

    if (text.isEmpty()) {
        emit analysisComplete();
        return;
    }

    emit analysisProgress(0);

    // Extract all words
    QStringList allWords = extractWords(text);
    int totalWordCount = allWords.size();

    core::Logger::getInstance().debug("WordFrequencyAnalyzer::analyzeText - extracted {} words",
                                      totalWordCount);

    // Count words and track positions
    int position = 0;
    int lastProgressPercent = 0;

    for (const QString& word : allWords) {
        // Update progress every 10%
        if (totalWordCount > 0) {
            int percent = (position * 100) / totalWordCount;
            if (percent >= lastProgressPercent + 10) {
                lastProgressPercent = percent;
                emit analysisProgress(percent);
            }
        }

        // Skip stop words if filtering enabled
        if (m_filterStopWords && isStopWord(word)) {
            ++position;
            continue;
        }

        m_wordCounts[word]++;
        m_wordPositions[word].append(position);
        ++m_totalWords;
        ++position;
    }

    // Build frequency list
    m_frequencies.clear();
    m_frequencies.reserve(m_wordCounts.size());

    for (auto it = m_wordCounts.begin(); it != m_wordCounts.end(); ++it) {
        WordFrequency freq;
        freq.word = it.key();
        freq.count = it.value();
        freq.percentage = (m_totalWords > 0)
            ? (100.0 * freq.count / m_totalWords)
            : 0.0;
        freq.isOverused = (freq.percentage >= m_overuseThreshold);
        m_frequencies.append(freq);
    }

    // Sort by count descending
    std::sort(m_frequencies.begin(), m_frequencies.end());

    // Detect close repetitions
    detectCloseRepetitions();

    core::Logger::getInstance().info("WordFrequencyAnalyzer::analyzeText - complete: {} total, {} unique, {} overused, {} repetitions",
                                     m_totalWords, m_wordCounts.size(),
                                     overusedWords().size(), m_repetitions.size());

    emit analysisProgress(100);
    emit analysisComplete();
}

QList<WordFrequency> WordFrequencyAnalyzer::frequencies() const
{
    return m_frequencies;
}

QList<WordFrequency> WordFrequencyAnalyzer::topWords(int n) const
{
    if (n <= 0 || m_frequencies.isEmpty()) {
        return {};
    }

    int count = std::min(n, static_cast<int>(m_frequencies.size()));
    return m_frequencies.mid(0, count);
}

QList<WordFrequency> WordFrequencyAnalyzer::overusedWords() const
{
    QList<WordFrequency> result;

    for (const auto& freq : m_frequencies) {
        if (freq.isOverused) {
            result.append(freq);
        }
    }

    return result;
}

QList<CloseRepetition> WordFrequencyAnalyzer::closeRepetitions() const
{
    return m_repetitions;
}

int WordFrequencyAnalyzer::totalWordCount() const
{
    return m_totalWords;
}

int WordFrequencyAnalyzer::uniqueWordCount() const
{
    return m_wordCounts.size();
}

// =============================================================================
// Word Lookup
// =============================================================================

WordFrequency WordFrequencyAnalyzer::frequencyOf(const QString& word) const
{
    QString lowercaseWord = word.toLower();

    // Find in frequencies list
    for (const auto& freq : m_frequencies) {
        if (freq.word == lowercaseWord) {
            return freq;
        }
    }

    // Not found - return empty
    WordFrequency empty;
    empty.word = lowercaseWord;
    return empty;
}

QList<int> WordFrequencyAnalyzer::positionsOf(const QString& word) const
{
    QString lowercaseWord = word.toLower();
    return m_wordPositions.value(lowercaseWord);
}

bool WordFrequencyAnalyzer::isStopWord(const QString& word) const
{
    QString lowercaseWord = word.toLower();

    // Check current language's stop words
    if (m_stopWords.contains(m_language)) {
        return m_stopWords[m_language].contains(lowercaseWord);
    }

    // Fallback to English
    if (m_stopWords.contains("en")) {
        return m_stopWords["en"].contains(lowercaseWord);
    }

    return false;
}

// =============================================================================
// Private Methods
// =============================================================================

void WordFrequencyAnalyzer::buildStopWordLists()
{
    // English stop words - common words that don't add meaning
    m_stopWords["en"] = {
        "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for",
        "of", "with", "by", "from", "as", "is", "was", "are", "were", "been",
        "be", "have", "has", "had", "do", "does", "did", "will", "would",
        "could", "should", "may", "might", "must", "shall", "can", "need",
        "it", "its", "this", "that", "these", "those", "i", "you", "he",
        "she", "we", "they", "me", "him", "her", "us", "them", "my", "your",
        "his", "our", "their", "what", "which", "who", "whom", "whose",
        "where", "when", "why", "how", "all", "each", "every", "both",
        "few", "more", "most", "other", "some", "such", "no", "not", "only",
        "same", "so", "than", "too", "very", "just", "also", "now", "here",
        "there", "then", "if", "about", "into", "through", "during", "before",
        "after", "above", "below", "between", "under", "again", "once",
        "any", "because", "being", "down", "further", "herself", "himself",
        "itself", "myself", "ourselves", "themselves", "yourself", "yourselves",
        "off", "out", "over", "own", "up", "while", "against", "am", "aren",
        "couldn", "didn", "doesn", "don", "hadn", "hasn", "haven", "isn",
        "ll", "mightn", "mustn", "needn", "shan", "shouldn", "ve", "wasn",
        "weren", "won", "wouldn", "s", "t", "d", "m", "re"
    };

    // Polish stop words
    m_stopWords["pl"] = {
        "i", "w", "z", "na", "do", "o", "ze", "to", "nie", "sie", "co",
        "jak", "ale", "po", "tak", "za", "od", "juz", "czy", "gdy", "go",
        "je", "jego", "jej", "ich", "tylko", "lub", "przez", "przy", "tym",
        "oraz", "ten", "ta", "te", "tej", "tego", "tych", "byc", "jest",
        "sa", "byl", "byla", "bylo", "bedzie", "a", "jako", "tez", "wiec",
        "aby", "jednak", "moze", "mozna", "mi", "mnie", "my", "nas", "ty",
        "ci", "wy", "was", "on", "ona", "ono", "oni", "one", "sobie", "siebie",
        "sie", "ze", "czy", "bo", "gdyz", "poniewaz", "ktory", "ktora", "ktore",
        "ktorzy", "u", "bardzo", "bez", "dla", "jeszcze", "juz", "kiedy",
        "niech", "pod", "przed", "nad", "miedzy", "razem", "wszystko",
        "nic", "kto", "nigdy", "zawsze", "teraz", "tutaj", "tam", "wszyscy",
        "kazdy", "kazda", "swoj", "swoja", "swoje", "twoj", "twoja", "twoje"
    };

    core::Logger::getInstance().debug("WordFrequencyAnalyzer - built stop word lists: en={}, pl={}",
                                      m_stopWords["en"].size(), m_stopWords["pl"].size());
}

QStringList WordFrequencyAnalyzer::extractWords(const QString& text) const
{
    QStringList words;

    if (text.isEmpty()) {
        return words;
    }

    // Match Unicode letters (\\p{L}+) - handles Polish, English, and other languages
    // This regex captures sequences of letters only
    static QRegularExpression wordRe("\\b(\\p{L}+)\\b",
                                     QRegularExpression::UseUnicodePropertiesOption);

    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        QString word = match.captured().toLower();

        // Skip single letters (too short to be meaningful)
        if (word.length() >= 2) {
            words.append(word);
        }
    }

    return words;
}

void WordFrequencyAnalyzer::detectCloseRepetitions()
{
    m_repetitions.clear();

    for (auto it = m_wordPositions.begin(); it != m_wordPositions.end(); ++it) {
        const QString& word = it.key();
        const QList<int>& positions = it.value();

        // Skip words with only one occurrence
        if (positions.size() < 2) {
            continue;
        }

        // Check consecutive positions for close repetitions
        for (int i = 1; i < positions.size(); ++i) {
            int distance = positions[i] - positions[i - 1];

            if (distance <= m_repetitionDistance) {
                CloseRepetition rep;
                rep.word = word;
                rep.firstPos = positions[i - 1];
                rep.secondPos = positions[i];
                rep.distance = distance;
                m_repetitions.append(rep);
            }
        }
    }

    // Sort by distance (closest first)
    std::sort(m_repetitions.begin(), m_repetitions.end(),
        [](const CloseRepetition& a, const CloseRepetition& b) {
            return a.distance < b.distance;
        });

    core::Logger::getInstance().debug("WordFrequencyAnalyzer::detectCloseRepetitions - found {} repetitions",
                                      m_repetitions.size());
}

}  // namespace kalahari::editor
