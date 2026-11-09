/// @file writing_activity.h
/// @brief The record of writing: the words written and deleted, and the time of writing, in
/// each 5-minute slot of a day, in each text of each book
///
/// The statistics and the goals of writing are counted from this record: the graph of the day,
/// the weeks and months, the days in a row, the hours of writing. A project keeps the record of
/// its own book in project.db, and the writer's journal (WritingJournal) keeps the record of all
/// the books, for the goals of the writer that are not the goals of one book.

#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTime>

namespace kalahari::core {

/// @brief The length of a slot of the record, in minutes
inline constexpr int ACTIVITY_SLOT_MINUTES = 5;

/// @brief The slots of a day: 0 starts at 0:00, 1 at 0:05, ..., 287 at 23:55
inline constexpr int ACTIVITY_SLOTS_PER_DAY = 24 * 60 / ACTIVITY_SLOT_MINUTES;

/// @brief What was written in one text of one book in one slot of a day
struct WritingActivity {
    QDate date;              ///< The day of the calendar the slot is in
    int slot = 0;            ///< The slot of the day, 0 to ACTIVITY_SLOTS_PER_DAY - 1
    QString bookId;          ///< The id of the project the text is in
    QString elementId;       ///< The id of the text in the project; empty when not known
    int written = 0;         ///< Words written
    int deleted = 0;         ///< Words deleted
    qint64 activeMs = 0;     ///< Time of writing, without the pauses longer than the idle time

    /// @brief Nothing was written, deleted or done in it
    bool isEmpty() const { return written == 0 && deleted == 0 && activeMs == 0; }

    /// @brief The same slot of the same day, in the same text of the same book
    bool samePlace(const WritingActivity& other) const {
        return slot == other.slot && date == other.date && bookId == other.bookId &&
               elementId == other.elementId;
    }

    bool operator==(const WritingActivity&) const = default;
};

/// @brief The slot of the day that a time of the day is in
int activitySlotOf(const QTime& time);

/// @brief When a slot of a day starts; slot ACTIVITY_SLOTS_PER_DAY is 0:00 of the next day
QDateTime activitySlotStart(const QDate& date, int slot);

/// @brief Collects the words written and deleted and the time of writing into slots, until
/// they are taken to be saved
///
/// The times are given to it, so it counts the same in the program and in the tests. The time
/// of writing is the time between two changes of the text, when the pause between them is not
/// longer than the idle time; it goes to the slots it passes through. A change of more words
/// than the limit at once (pasting a chapter, cutting it, restoring a version) is not writing
/// and is not counted.
class WritingRecorder {
public:
    /// @brief The book and the text that the next changes are made in
    void setPlace(const QString& bookId, const QString& elementId);

    /// @brief The id of the book the next changes are made in
    const QString& bookId() const { return m_bookId; }

    /// @brief The id of the text the next changes are made in
    const QString& elementId() const { return m_elementId; }

    /// @brief The pause after which the time of writing stops (default 5 minutes)
    void setIdleSeconds(int seconds);

    /// @brief The changes of more words than @p words at once are not counted; 0 counts every
    /// change (default 500)
    void setLargeChangeWords(int words);

    /// @brief The writer changed the text at @p when: the time since the last change is time
    /// of writing when the pause is not longer than the idle time
    void noteActivity(const QDateTime& when);

    /// @brief The words of the text changed by @p delta at @p when: more words were written,
    /// fewer deleted
    /// @return false when the change is larger than the limit and was not counted
    bool recordWords(int delta, const QDateTime& when);

    /// @brief The next change starts a new stretch of writing: the time since the last change
    /// is not counted (a text was closed, the program was in the background)
    void pause();

    /// @brief What is collected and not taken yet, one entry for each slot, book and text
    const QList<WritingActivity>& pending() const { return m_pending; }

    /// @brief Takes what is collected, to be saved
    QList<WritingActivity> takePending();

    /// @brief Words written since the start or the last resetTotals()
    int written() const { return m_written; }

    /// @brief Words deleted since the start or the last resetTotals()
    int deleted() const { return m_deleted; }

    /// @brief Time of writing since the start or the last resetTotals()
    qint64 activeMs() const { return m_activeMs; }

    /// @brief Counts the totals from zero again; what is collected stays to be taken
    void resetTotals();

private:
    WritingActivity& entryAt(const QDateTime& when);
    void addTime(const QDateTime& from, const QDateTime& to);

    QString m_bookId;
    QString m_elementId;
    qint64 m_idleMs = 5 * 60 * 1000;
    int m_largeChangeWords = 500;
    QDateTime m_lastActivity;
    QList<WritingActivity> m_pending;
    int m_written = 0;
    int m_deleted = 0;
    qint64 m_activeMs = 0;
};

}  // namespace kalahari::core
