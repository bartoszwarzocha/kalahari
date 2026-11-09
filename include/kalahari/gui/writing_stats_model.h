/// @file writing_stats_model.h
/// @brief What the statistics bar, the Statistics panel and the goals show: the record of
/// writing counted by the writer's rules, with what the editor has not saved yet

#pragma once

#include <kalahari/core/writing_activity.h>
#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_journal.h>
#include <kalahari/core/writing_statistics.h>

#include <QDateTime>
#include <QObject>
#include <QPointer>

#include <functional>
#include <utility>

class QTimer;

namespace kalahari::core {
class ProjectDatabase;
}

namespace kalahari::editor {
class StatisticsCollector;
}

namespace kalahari::gui {

/// @brief A piece of a writing day on the graph of the bar: its words written and deleted
struct DayPiece {
    int written = 0;
    int deleted = 0;
    qint64 activeMs = 0;

    /// @brief The words that count toward the goals under @p counting
    int counted(core::GoalCounting counting) const {
        return counting == core::GoalCounting::Net ? written - deleted : written;
    }
};

/// @brief The statistics of writing of the writer and of the open book
///
/// Two records: the writer's journal (all the books, WritingJournal) and the record of the open
/// book in its project.db. Both are read once, then follow what the editor records
/// (StatisticsCollector): what it saved, and what it has not saved yet. The daily and weekly
/// goals count the journal or the book, as the rules say (WritingRules::allBooks).
///
/// The model tells when the numbers change (changed()), at most a few times a second, and once
/// a minute while time passes (the graph of the day moves on, the temperature cools down).
class WritingStatsModel : public QObject {
    Q_OBJECT

public:
    /// @brief changed() follows the edits at most this often
    static constexpr int CHANGE_INTERVAL_MS = 250;

    /// @brief The rules from the settings; the journal is not open yet
    explicit WritingStatsModel(QObject* parent = nullptr);
    ~WritingStatsModel() override;

    /// @brief Open the writer's journal and read it
    /// @param path Its file; WritingJournal::defaultPath() in the program
    bool openJournal(const QString& path);

    /// @brief The writer's journal (open or not)
    core::WritingJournal& journal() { return m_journal; }

    /// @brief Follow what @p collector records (nullptr: none); it saves to the journal, with
    /// the rules of the settings and in the open book
    void setCollector(editor::StatisticsCollector* collector);

    /// @brief The collector it follows
    editor::StatisticsCollector* collector() const;

    /// @brief The open book: its database and its id (nullptr: no book); its record is read
    /// and its goals with it
    void setBook(core::ProjectDatabase* database, const QString& bookId);

    /// @brief A book is open
    bool hasBook() const { return m_database != nullptr; }

    /// @brief The id of the open book; empty without one
    const QString& bookId() const { return m_bookId; }

    /// @brief How the words of the open book are counted now (the coordinator gives it)
    void setBookWordsSource(std::function<int()> source);

    /// @brief The words of the open book now; 0 without one
    int bookWords() const;

    /// @brief The goals of the open book; empty without one
    const core::BookGoals& goals() const { return m_goals; }

    /// @brief Keep new goals of the open book in its project.db
    /// @return false without a book or when they cannot be written
    bool setGoals(const core::BookGoals& goals);

    /// @brief The writer's rules (Settings > Statistics and Goals)
    const core::WritingRules& rules() const { return m_rules; }

    /// @brief Take the rules of the settings again (called when they change)
    void reloadRules();

    /// @brief The record the daily and weekly goals count, with what is not saved yet: of all
    /// the books or of the open one
    core::WritingStatistics goalStatistics() const;

    /// @brief The record of the open book, with what is not saved yet
    core::WritingStatistics bookStatistics() const;

    /// @brief The record of the journal, with what is not saved yet
    core::WritingStatistics journalStatistics() const;

    /// @brief Now; the tests set their own clock
    QDateTime now() const;

    /// @brief A clock of the tests; empty: the real one
    void setClock(std::function<QDateTime()> clock);

    /// @brief The writing day of now
    QDate today() const;

    /// @brief The pieces of @p minutes of a writing day of @p statistics, from its start (the
    /// end of the day of the rules); a multiple of ACTIVITY_SLOT_MINUTES that divides a day
    static QList<DayPiece> dayPieces(const core::WritingStatistics& statistics, const QDate& day,
                                     int minutes);

    /// @brief The minutes of a writing day that its graph shows, from the start of the day:
    /// the hours @p fromHour to @p toHour of the clock (24 is midnight), wider when @p pieces
    /// of @p pieceMinutes have writing outside them or @p nowMinute is outside; they start
    /// and end on whole hours of the clock
    /// @param dayStart The end of a writing day of the rules, in minutes after midnight
    /// @return The first minute and the minute after the last one
    static std::pair<int, int> shownMinutes(const QList<DayPiece>& pieces, int pieceMinutes,
                                            int dayStart, int fromHour, int toHour,
                                            int nowMinute);

    /// @brief The minutes from the start of writing day @p day to @p when; may be outside
    /// 0 to 1440
    qint64 minutesIntoDay(const QDate& day, const QDateTime& when) const;

    /// @brief The words of the daily goal today: the daily goal of the rules, or what the
    /// deadline of the book needs when its goals say so
    int dailyGoalToday() const;

signals:
    /// @brief The numbers changed: something was written, saved or the time went on
    void changed();

    /// @brief The goals of the book changed (setGoals(), another book)
    void goalsChanged();

private:
    /// @brief changed() soon, at most every CHANGE_INTERVAL_MS
    void schedule();

    /// @brief Read the record of the open book and its goals
    void readBook();

    core::WritingRules m_rules;
    core::WritingJournal m_journal;
    core::WritingStatistics m_journalRecord;  ///< Saved in the journal
    core::WritingStatistics m_bookRecord;     ///< Saved in the book
    core::BookGoals m_goals;
    core::ProjectDatabase* m_database{nullptr};
    QString m_bookId;
    QPointer<editor::StatisticsCollector> m_collector;
    std::function<int()> m_bookWords;
    std::function<QDateTime()> m_clock;
    QTimer* m_changeTimer{nullptr};  ///< schedule()
    QTimer* m_minuteTimer{nullptr};  ///< The time goes on
    int m_settingsListener{-1};
};

}  // namespace kalahari::gui
