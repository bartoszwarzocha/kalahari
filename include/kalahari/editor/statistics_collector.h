/// @file statistics_collector.h
/// @brief Real-time document statistics collection and session tracking (OpenSpec #00042 Phase 6)
///
/// StatisticsCollector provides:
/// - Real-time word/character/paragraph counting
/// - Writing session tracking (words written/deleted, active time)
/// - Database integration for historical statistics
/// - Automatic periodic flush to database
///
/// The collector uses an observer pattern to track document changes
/// and maintains hourly statistics for productivity analysis.

#pragma once

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QString>

// Forward declarations
namespace kalahari::editor {
class KmlDocument;
class IDocumentObserver;
}

namespace kalahari::core {
class ProjectDatabase;
}

namespace kalahari::editor {

/// @brief Real-time statistics collector for document editing
///
/// Tracks document statistics (words, characters, paragraphs) and session
/// metrics (words written/deleted, active time). Integrates with ProjectDatabase
/// for persistent storage of hourly statistics.
///
/// Usage:
/// @code
/// auto collector = new StatisticsCollector(this);
/// collector->setDocument(document);
/// collector->setDatabase(database);
/// collector->startSession();
///
/// // Connect to statistics updates
/// connect(collector, &StatisticsCollector::statisticsChanged,
///         this, &MyWidget::updateStatusBar);
/// @endcode
class StatisticsCollector : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a statistics collector
    /// @param parent Parent QObject for ownership
    explicit StatisticsCollector(QObject* parent = nullptr);

    /// @brief Destructor - automatically ends session if active
    ~StatisticsCollector() override;

    // Non-copyable
    StatisticsCollector(const StatisticsCollector&) = delete;
    StatisticsCollector& operator=(const StatisticsCollector&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the document to track
    /// @param document KML document to monitor (nullptr to disconnect)
    /// @note Previous document is automatically disconnected
    void setDocument(KmlDocument* document);

    /// @brief Set the database for statistics persistence
    /// @param database ProjectDatabase for storing session stats (nullptr to disable)
    void setDatabase(core::ProjectDatabase* database);

    // =========================================================================
    // Real-time Statistics (from current document)
    // =========================================================================

    /// @brief Get the word count of the current document
    /// @return Number of words (0 if no document)
    int wordCount() const;

    /// @brief Get the character count including spaces
    /// @return Number of characters (0 if no document)
    int characterCount() const;

    /// @brief Get the character count excluding spaces
    /// @return Number of characters without spaces (0 if no document)
    int characterCountNoSpaces() const;

    /// @brief Get the paragraph count
    /// @return Number of paragraphs (0 if no document)
    int paragraphCount() const;

    /// @brief Get estimated reading time in minutes
    /// @return Minutes at 200 words per minute
    int estimatedReadingTime() const;

    // =========================================================================
    // Session Tracking
    // =========================================================================

    /// @brief Start a new writing session
    /// @note If a session is already active, this is a no-op
    void startSession();

    /// @brief End the current writing session
    /// @note Flushes statistics to database before ending
    void endSession();

    /// @brief Check if a session is currently active
    /// @return true if session is active
    bool isSessionActive() const;

    /// @brief Force flush statistics to database
    /// @note Normally called automatically by timer
    void flush();

    // =========================================================================
    // Session Statistics Getters
    // =========================================================================

    /// @brief Get words written in current session
    /// @return Number of new words added this session
    int wordsWrittenThisSession() const;

    /// @brief Get words deleted in current session
    /// @return Number of words removed this session
    int wordsDeletedThisSession() const;

    /// @brief Get active editing time in current session
    /// @return Minutes of active editing (excludes idle time)
    int activeMinutesThisSession() const;

    /// @brief Get session duration in minutes
    /// @return Total minutes since session start
    int sessionDurationMinutes() const;

signals:
    /// @brief Emitted when statistics change
    /// @param words Current word count
    /// @param chars Current character count
    /// @param paragraphs Current paragraph count
    void statisticsChanged(int words, int chars, int paragraphs);

    /// @brief Emitted when session statistics are updated
    /// @param wordsWritten Total words written this session
    /// @param wordsDeleted Total words deleted this session
    /// @param activeMinutes Active editing minutes this session
    void sessionStatsUpdated(int wordsWritten, int wordsDeleted, int activeMinutes);

private slots:
    /// @brief Handle flush timer timeout
    void onFlushTimer();

private:
    // Document observer implementation
    class DocumentObserver;
    friend class DocumentObserver;

    /// @brief Called when document content changes
    void onDocumentChanged();

    /// @brief Recalculate all statistics from document
    void recalculateStats();

    /// @brief Update hourly stats with word delta
    /// @param wordsDelta Change in word count (positive = written, negative = deleted)
    void updateHourlyStats(int wordsDelta);

    /// @brief Check if hour has changed and handle rollover
    void checkHourRollover();

    /// @brief Count words in a text string
    /// @param text Text to count words in
    /// @return Number of words
    int countWordsInText(const QString& text) const;

    /// @brief Save current hour's stats to database
    void saveHourlyStats();

    /// @brief Update active time tracking
    void updateActiveTime();

    // Document and database
    KmlDocument* m_document{nullptr};
    core::ProjectDatabase* m_database{nullptr};
    std::unique_ptr<DocumentObserver> m_observer;

    // Cached statistics (real-time)
    int m_wordCount{0};
    int m_characterCount{0};
    int m_characterCountNoSpaces{0};

    // Session tracking
    bool m_sessionActive{false};
    QDateTime m_sessionStart;
    int m_previousWordCount{0};  ///< For delta calculation
    int m_wordsWritten{0};       ///< Total session words written
    int m_wordsDeleted{0};       ///< Total session words deleted

    // Hourly tracking (for database)
    int m_currentHour{-1};
    int m_wordsWrittenThisHour{0};
    int m_wordsDeletedThisHour{0};
    int m_activeMinutesThisHour{0};
    QDateTime m_lastActivityTime;

    // Auto-flush timer
    QTimer* m_flushTimer{nullptr};

    // Constants
    static constexpr int FLUSH_INTERVAL_MS = 5 * 60 * 1000;  ///< 5 minutes
    static constexpr int IDLE_THRESHOLD_MS = 2 * 60 * 1000;  ///< 2 minutes
    static constexpr int WORDS_PER_MINUTE = 200;             ///< Reading speed
};

}  // namespace kalahari::editor
