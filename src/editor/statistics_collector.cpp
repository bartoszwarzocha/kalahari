/// @file statistics_collector.cpp
/// @brief Real-time document statistics collection implementation (OpenSpec #00042 Phase 6)

#include <kalahari/editor/statistics_collector.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/database_types.h>
#include <kalahari/core/logger.h>

#include <QRegularExpression>

namespace kalahari::editor {

// =============================================================================
// StatisticsCollector
// =============================================================================

StatisticsCollector::StatisticsCollector(QObject* parent)
    : QObject(parent)
    , m_flushTimer(new QTimer(this))
{
    // Setup auto-flush timer
    m_flushTimer->setInterval(FLUSH_INTERVAL_MS);
    connect(m_flushTimer, &QTimer::timeout, this, &StatisticsCollector::onFlushTimer);

    core::Logger::getInstance().debug("StatisticsCollector created");
}

StatisticsCollector::~StatisticsCollector()
{
    // End session if active (will flush to database)
    if (m_sessionActive) {
        endSession();
    }

    // Disconnect from editor
    if (m_editor) {
        disconnect(m_editor, &BookEditor::contentChanged,
                   this, &StatisticsCollector::onContentChanged);
    }

    core::Logger::getInstance().debug("StatisticsCollector destroyed");
}

// =============================================================================
// Setup
// =============================================================================

void StatisticsCollector::setBookEditor(BookEditor* editor)
{
    if (m_editor == editor) {
        return;
    }

    // Disconnect from previous editor
    if (m_editor) {
        disconnect(m_editor, &BookEditor::contentChanged,
                   this, &StatisticsCollector::onContentChanged);
    }

    m_editor = editor;

    // Connect to new editor
    if (m_editor) {
        connect(m_editor, &BookEditor::contentChanged,
                this, &StatisticsCollector::onContentChanged);
        recalculateStats();
        m_previousWordCount = m_wordCount;
    } else {
        // Reset statistics when no editor
        m_wordCount = 0;
        m_characterCount = 0;
        m_characterCountNoSpaces = 0;
    }

    emit statisticsChanged(m_wordCount, m_characterCount,
                          m_editor ? static_cast<int>(m_editor->paragraphCount()) : 0);
}

void StatisticsCollector::setDatabase(core::ProjectDatabase* database)
{
    m_database = database;

    if (m_database) {
        core::Logger::getInstance().debug("StatisticsCollector connected to database");
    }
}

// =============================================================================
// Real-time Statistics
// =============================================================================

int StatisticsCollector::wordCount() const
{
    return m_wordCount;
}

int StatisticsCollector::characterCount() const
{
    return m_characterCount;
}

int StatisticsCollector::characterCountNoSpaces() const
{
    return m_characterCountNoSpaces;
}

int StatisticsCollector::paragraphCount() const
{
    return m_editor ? static_cast<int>(m_editor->paragraphCount()) : 0;
}

int StatisticsCollector::estimatedReadingTime() const
{
    if (m_wordCount == 0) {
        return 0;
    }

    // Reading time in minutes at WORDS_PER_MINUTE
    // Round up to nearest minute
    return (m_wordCount + WORDS_PER_MINUTE - 1) / WORDS_PER_MINUTE;
}

// =============================================================================
// Session Tracking
// =============================================================================

void StatisticsCollector::startSession()
{
    if (m_sessionActive) {
        core::Logger::getInstance().debug("StatisticsCollector::startSession - session already active");
        return;
    }

    m_sessionActive = true;
    m_sessionStart = QDateTime::currentDateTime();
    m_lastActivityTime = m_sessionStart;

    // Reset session counters
    m_wordsWritten = 0;
    m_wordsDeleted = 0;

    // Initialize hour tracking
    m_currentHour = m_sessionStart.time().hour();
    m_wordsWrittenThisHour = 0;
    m_wordsDeletedThisHour = 0;
    m_activeMinutesThisHour = 0;

    // Capture current word count for delta calculation
    m_previousWordCount = m_wordCount;

    // Start auto-flush timer
    m_flushTimer->start();

    core::Logger::getInstance().info("Writing session started");
}

void StatisticsCollector::endSession()
{
    if (!m_sessionActive) {
        return;
    }

    // Stop timer first
    m_flushTimer->stop();

    // Final flush to database
    flush();

    m_sessionActive = false;

    core::Logger::getInstance().info("Writing session ended - words written: {}, deleted: {}, active minutes: {}",
                                     m_wordsWritten, m_wordsDeleted, activeMinutesThisSession());
}

bool StatisticsCollector::isSessionActive() const
{
    return m_sessionActive;
}

void StatisticsCollector::flush()
{
    if (!m_sessionActive) {
        return;
    }

    // Update active time before saving
    updateActiveTime();

    // Save to database if available
    saveHourlyStats();

    core::Logger::getInstance().debug("StatisticsCollector flushed - hour {}: written={}, deleted={}, active={}min",
                                      m_currentHour, m_wordsWrittenThisHour, m_wordsDeletedThisHour, m_activeMinutesThisHour);
}

// =============================================================================
// Session Statistics Getters
// =============================================================================

int StatisticsCollector::wordsWrittenThisSession() const
{
    return m_wordsWritten;
}

int StatisticsCollector::wordsDeletedThisSession() const
{
    return m_wordsDeleted;
}

int StatisticsCollector::activeMinutesThisSession() const
{
    if (!m_sessionActive) {
        return 0;
    }

    // Calculate based on session duration minus idle time
    // This is an approximation - the accurate count is tracked per hour
    // For UI display purposes, we can use this rough estimate
    // or sum up all the hourly active minutes

    // For now, return based on total session time with idle detection
    // In practice, the accurate count comes from periodic updateActiveTime() calls
    qint64 sessionMs = m_sessionStart.msecsTo(QDateTime::currentDateTime());
    return static_cast<int>(sessionMs / 60000);  // Simple approximation
}

int StatisticsCollector::sessionDurationMinutes() const
{
    if (!m_sessionActive) {
        return 0;
    }

    qint64 durationMs = m_sessionStart.msecsTo(QDateTime::currentDateTime());
    return static_cast<int>(durationMs / 60000);
}

// =============================================================================
// Private Slots
// =============================================================================

void StatisticsCollector::onFlushTimer()
{
    flush();
}

// =============================================================================
// Private Methods
// =============================================================================

void StatisticsCollector::onContentChanged()
{
    // Store previous counts
    int previousWords = m_wordCount;

    // Recalculate all stats
    recalculateStats();

    // Calculate delta for session tracking
    if (m_sessionActive) {
        int wordsDelta = m_wordCount - previousWords;
        updateHourlyStats(wordsDelta);

        // Update activity timestamp (user is actively editing)
        m_lastActivityTime = QDateTime::currentDateTime();
    }

    // Emit statistics changed signal
    emit statisticsChanged(m_wordCount, m_characterCount, paragraphCount());
}

void StatisticsCollector::recalculateStats()
{
    if (!m_editor) {
        m_wordCount = 0;
        m_characterCount = 0;
        m_characterCountNoSpaces = 0;
        return;
    }

    // Get plain text from editor (uses QTextDocument API)
    QString text = m_editor->plainText();

    // Character counts
    m_characterCount = text.length();
    m_characterCountNoSpaces = text.count(QRegularExpression("[^ \\t\\n\\r]"));

    // Word count
    m_wordCount = countWordsInText(text);
}

void StatisticsCollector::updateHourlyStats(int wordsDelta)
{
    // Check for hour rollover first
    checkHourRollover();

    // Update session totals
    if (wordsDelta > 0) {
        m_wordsWritten += wordsDelta;
        m_wordsWrittenThisHour += wordsDelta;
    } else if (wordsDelta < 0) {
        int deleted = -wordsDelta;
        m_wordsDeleted += deleted;
        m_wordsDeletedThisHour += deleted;
    }

    // Update previous word count for next delta
    m_previousWordCount = m_wordCount;

    // Emit session stats update
    emit sessionStatsUpdated(m_wordsWritten, m_wordsDeleted, activeMinutesThisSession());
}

void StatisticsCollector::checkHourRollover()
{
    int currentHour = QDateTime::currentDateTime().time().hour();

    if (m_currentHour != currentHour && m_currentHour >= 0) {
        // Hour changed - save previous hour's stats
        core::Logger::getInstance().debug("Hour rollover: {} -> {}", m_currentHour, currentHour);

        saveHourlyStats();

        // Reset hourly counters
        m_currentHour = currentHour;
        m_wordsWrittenThisHour = 0;
        m_wordsDeletedThisHour = 0;
        m_activeMinutesThisHour = 0;
    }
}

int StatisticsCollector::countWordsInText(const QString& text) const
{
    if (text.isEmpty()) {
        return 0;
    }

    // Use regex to match word boundaries
    // This handles Unicode and various word patterns
    static QRegularExpression wordRe("\\b\\w+\\b");
    auto matches = wordRe.globalMatch(text);

    int count = 0;
    while (matches.hasNext()) {
        matches.next();
        ++count;
    }

    return count;
}

void StatisticsCollector::saveHourlyStats()
{
    if (!m_database || !m_database->isOpen()) {
        return;
    }

    // Only save if there's something to save
    if (m_wordsWrittenThisHour == 0 && m_wordsDeletedThisHour == 0 && m_activeMinutesThisHour == 0) {
        return;
    }

    core::SessionStats stats;
    stats.timestamp = QDateTime::currentDateTime();
    stats.documentId = QString();  // TODO: Get document ID if available
    stats.wordsWritten = m_wordsWrittenThisHour;
    stats.wordsDeleted = m_wordsDeletedThisHour;
    stats.activeMinutes = m_activeMinutesThisHour;
    stats.hour = m_currentHour;

    m_database->recordSessionStats(stats);

    core::Logger::getInstance().debug("Saved hourly stats to database: hour={}, written={}, deleted={}, active={}min",
                                      m_currentHour, m_wordsWrittenThisHour, m_wordsDeletedThisHour, m_activeMinutesThisHour);
}

void StatisticsCollector::updateActiveTime()
{
    if (!m_sessionActive) {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    qint64 msSinceLastActivity = m_lastActivityTime.msecsTo(now);

    // If last activity was within idle threshold, count time as active
    if (msSinceLastActivity <= IDLE_THRESHOLD_MS) {
        // Calculate minutes since last check
        // For simplicity, we increment by 1 minute per flush (5 minutes)
        // More accurate tracking would require a more frequent timer
        m_activeMinutesThisHour += 1;
    }
}

}  // namespace kalahari::editor
