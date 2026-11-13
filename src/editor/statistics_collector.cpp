/// @file statistics_collector.cpp
/// @brief Real-time document statistics collection implementation (OpenSpec #00042 Phase 6)

#include <kalahari/editor/statistics_collector.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/writing_journal.h>
#include <kalahari/core/writing_statistics.h>

#include <QVariant>

#include <algorithm>
#include <cstdlib>

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

    // Create debounce timer for statistics calculation
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &StatisticsCollector::debouncedRecalculate);

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
        disconnect(m_editor, nullptr, this, nullptr);
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

    // The last edits in the previous editor still count to the session, in its text
    if (m_editor) {
        if (m_debounceTimer->isActive()) {
            m_debounceTimer->stop();
            debouncedRecalculate();
        }
        disconnect(m_editor, nullptr, this, nullptr);
        // Its pastes keep their words while it keeps their undo: until it shows another text
        // or is closed
        if (m_pastedAtLevel.contains(m_editor)) {
            connect(m_editor, &QObject::destroyed, this, &StatisticsCollector::onEditorDestroyed,
                    Qt::UniqueConnection);
        }
    }

    m_editor = editor;
    m_elementId = elementIdOf(editor);
    m_recorder.setPlace(m_bookId, m_elementId);

    if (m_editor) {
        connect(m_editor, &BookEditor::contentChanged,
                this, &StatisticsCollector::onContentChanged);
        // The words pasted are kept apart from the words written
        connect(m_editor, &BookEditor::pastingStarted, this,
                &StatisticsCollector::countPendingChange);
        connect(m_editor, &BookEditor::pastingFinished, this,
                &StatisticsCollector::onPastingFinished);
        // Text moved to or from another text (Book > Chapter Break) is not written or deleted
        connect(m_editor, &BookEditor::movingStarted, this,
                &StatisticsCollector::countPendingChange);
        connect(m_editor, &BookEditor::movingFinished, this, &StatisticsCollector::rebase);
        // A newly loaded text, or new rules of counting, are not words written or deleted; a
        // new text has no undo, and no pastes to undo
        connect(m_editor, &BookEditor::documentChanged, this, [this]() {
            m_pastedAtLevel.remove(m_editor);
            rebase();
        });
        connect(m_editor, &BookEditor::countsChanged, this, &StatisticsCollector::rebase);
        // An editor closed while it is followed is forgotten
        connect(m_editor, &QObject::destroyed, this, &StatisticsCollector::onEditorDestroyed,
                Qt::UniqueConnection);
    }
    rebase();
}

void StatisticsCollector::setDatabase(core::ProjectDatabase* database)
{
    // What was written in the book goes to its database before another one is set
    if (database != m_database) {
        flush();
    }
    m_database = database;

    if (m_database) {
        core::Logger::getInstance().debug("StatisticsCollector connected to database");
    }
}

void StatisticsCollector::setJournal(core::WritingJournal* journal)
{
    if (journal != m_journal) {
        flush();
    }
    m_journal = journal;
}

void StatisticsCollector::setBookId(const QString& bookId)
{
    m_bookId = bookId;
    m_recorder.setPlace(m_bookId, m_elementId);
}

void StatisticsCollector::setRules(const core::WritingRules& rules)
{
    m_recorder.setIdleSeconds(rules.idleMinutes * 60);
    m_recorder.setLargeChangeWords(rules.largeChangeWords);
}

const QList<core::WritingActivity>& StatisticsCollector::pendingActivity() const
{
    return m_recorder.pending();
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
    return m_paragraphCount;
}

int StatisticsCollector::estimatedReadingTime() const
{
    return core::readingMinutes(m_wordCount);
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

    // Reset session counters; the time of writing starts with the first change
    m_recorder.resetTotals();
    m_recorder.pause();

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
    m_recorder.pause();

    core::Logger::getInstance().info("Writing session ended - words written: {}, deleted: {}, active minutes: {}",
                                     m_recorder.written(), m_recorder.deleted(),
                                     activeMinutesThisSession());
}

bool StatisticsCollector::isSessionActive() const
{
    return m_sessionActive;
}

void StatisticsCollector::flush()
{
    if (m_recorder.pending().isEmpty()) {
        return;
    }

    // The record goes to the book and to the journal of all the books; what one of them
    // cannot take is lost to it only, and the other keeps it
    const QList<core::WritingActivity> activity = m_recorder.takePending();
    if (m_database && m_database->isOpen() && !m_database->addWritingActivity(activity)) {
        core::Logger::getInstance().warn("StatisticsCollector: the record of writing was not "
                                         "saved in the book: {}",
                                         m_database->lastError().toStdString());
    }
    if (m_journal && m_journal->isOpen() && !m_journal->add(activity)) {
        core::Logger::getInstance().warn("StatisticsCollector: the record of writing was not "
                                         "saved in the journal: {}",
                                         m_journal->lastError().toStdString());
    }
    emit activitySaved(activity);

    core::Logger::getInstance().debug("StatisticsCollector flushed {} entries of the record of writing",
                                      activity.size());
}

// =============================================================================
// Session Statistics Getters
// =============================================================================

int StatisticsCollector::wordsWrittenThisSession() const
{
    return m_recorder.written();
}

int StatisticsCollector::wordsDeletedThisSession() const
{
    return m_recorder.deleted();
}

int StatisticsCollector::wordsPastedThisSession() const
{
    return m_recorder.pasted();
}

int StatisticsCollector::activeMinutesThisSession() const
{
    return static_cast<int>(activeMsThisSession() / 60000);
}

qint64 StatisticsCollector::activeMsThisSession() const
{
    return m_sessionActive ? m_recorder.activeMs() : 0;
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
    // Debounce: restart timer on each change
    // Stats will be calculated 250ms after last keystroke
    // This prevents O(N) stats calculation on every keystroke
    if (m_debounceTimer) {
        m_debounceTimer->start(STATS_DEBOUNCE_MS);
    }

    // The time of writing runs from change to change (cheap operation)
    if (m_sessionActive) {
        m_recorder.noteActivity(QDateTime::currentDateTime());
        emit writingRecorded();
    }
}

void StatisticsCollector::debouncedRecalculate()
{
    // Store previous word count for delta calculation
    int previousWords = m_wordCount;

    // Recalculate all statistics
    recalculateStats();

    // Calculate word delta for session tracking (only if session active)
    if (m_sessionActive) {
        const int wordsDelta = m_wordCount - previousWords;
        if (wordsDelta != 0) {
            m_previousWordCount = m_wordCount;
            // Pasting or cutting a chapter, or restoring a version, is not writing
            if (m_recorder.recordWords(wordsDelta, QDateTime::currentDateTime())) {
                emit writingRecorded();
                emit sessionStatsUpdated(m_recorder.written(), m_recorder.deleted(),
                                         activeMinutesThisSession());
            } else {
                core::Logger::getInstance().info(
                    "StatisticsCollector: a change of {} words at once is not counted as writing",
                    wordsDelta);
                emit largeChangeSkipped(std::abs(wordsDelta));
            }
        }
    }

    // Emit statistics changed
    emit statisticsChanged(m_wordCount, m_characterCount, paragraphCount());
}

void StatisticsCollector::countPendingChange()
{
    if (m_debounceTimer->isActive()) {
        m_debounceTimer->stop();
        debouncedRecalculate();
    }
}

void StatisticsCollector::onPastingFinished(int level, bool undoOrRedo)
{
    m_debounceTimer->stop();
    const int previousWords = m_wordCount;
    recalculateStats();
    const int wordsDelta = m_wordCount - previousWords;
    if (!undoOrRedo) {
        m_pastedAtLevel[m_editor].insert(level, std::max(0, wordsDelta));
    }
    if (m_sessionActive && wordsDelta != 0) {
        m_previousWordCount = m_wordCount;
        const QDateTime now = QDateTime::currentDateTime();
        // Pasted: the words it brought. A paste that replaced more words than it brought
        // deleted words written by hand. Undone and redone, a paste takes away and brings back
        // its words, and the words typed right after it in its level of undo are deleted and
        // written again.
        int pasted = 0;
        if (!undoOrRedo) {
            pasted = std::max(0, wordsDelta);
        } else {
            const int words = m_pastedAtLevel.value(m_editor).value(level, std::abs(wordsDelta));
            pasted = wordsDelta > 0 ? std::min(wordsDelta, words) : std::max(wordsDelta, -words);
        }
        m_recorder.recordPasted(pasted, now);
        if (wordsDelta != pasted && !m_recorder.recordWords(wordsDelta - pasted, now)) {
            emit largeChangeSkipped(std::abs(wordsDelta - pasted));
        }
        emit writingRecorded();
        emit sessionStatsUpdated(m_recorder.written(), m_recorder.deleted(),
                                 activeMinutesThisSession());
    }
    emit statisticsChanged(m_wordCount, m_characterCount, paragraphCount());
}

void StatisticsCollector::rebase()
{
    // The counts of now are where the next words written or deleted are counted from
    m_debounceTimer->stop();
    recalculateStats();
    m_previousWordCount = m_wordCount;
    emit statisticsChanged(m_wordCount, m_characterCount, paragraphCount());
}

void StatisticsCollector::onEditorDestroyed(QObject* object)
{
    m_pastedAtLevel.remove(object);
    if (object == m_editor) {
        m_editor = nullptr;
        rebase();
    }
}

void StatisticsCollector::recalculateStats()
{
    if (!m_editor) {
        m_wordCount = 0;
        m_characterCount = 0;
        m_characterCountNoSpaces = 0;
        m_paragraphCount = 0;
        return;
    }

    // The editor caches the counts per paragraph, so only edited paragraphs are counted
    const core::TextCounts counts = m_editor->textCounts();
    m_wordCount = counts.words;
    m_characterCount = counts.characters;
    m_characterCountNoSpaces = counts.nonSpaceCharacters;
    m_paragraphCount = counts.paragraphs;
}

QString StatisticsCollector::elementIdOf(const BookEditor* editor)
{
    // The panel of a chapter knows its element; the editor is somewhere inside it
    for (const QObject* object = editor; object != nullptr; object = object->parent()) {
        const QVariant id = object->property("elementId");
        if (id.isValid()) {
            return id.toString();
        }
    }
    return {};
}

}  // namespace kalahari::editor
