// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_stats_model.h
/// @brief What the statistics bar, the Statistics panel and the goals show: the record of
/// writing counted by the writer's rules, with what the editor has not saved yet

#ifndef KALAHARI_GUI_WRITING_STATS_MODEL_H
#define KALAHARI_GUI_WRITING_STATS_MODEL_H

#include <kalahari/core/writing_activity.h>
#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_plan.h>
#include <kalahari/core/writing_journal.h>
#include <kalahari/core/writing_statistics.h>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <utility>

class QTimer;

namespace kalahari::core {
class ProjectDatabase;
} // namespace kalahari::core

namespace kalahari::editor {
class StatisticsCollector;
} // namespace kalahari::editor

namespace kalahari::gui {

/// @brief A piece of a writing day on the graph of the bar: its words written and deleted
struct DayPiece {
    int written = 0;
    int deleted = 0;
    qint64 activeMs = 0;

    /// @brief The words that count toward the goals under @p counting
    [[nodiscard]] int counted(core::GoalCounting counting) const {
        return counting == core::GoalCounting::Net ? written - deleted : written;
    }
};

/// @brief Where the daily goal of today comes from
enum class DailyGoalSource {
    Own,       ///< The writer's own daily goal (Settings > Statistics and Goals)
    Deadline,  ///< What the deadline of the book needs (core::dailyGoalForDeadline())
    Schedule,  ///< What the schedule of the book needs (core::dailyGoalForPlan())
};

/// @brief The statistics of writing of the writer and of the open book
///
/// Two records: the writer's journal (all the books, WritingJournal) and the record of the open
/// book in its project.db. Both are read once, then follow what the editor records
/// (StatisticsCollector): what it saved, and what it has not saved yet. The daily and weekly
/// goals count the journal or the book, as the rules say (WritingRules::allBooks).
///
/// The model tells when the numbers change (changed()), at most a few times a second, and once
/// a minute while time passes (the graph of the day moves on, the pace of work cools down).
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
    [[nodiscard]] editor::StatisticsCollector* collector() const;

    /// @brief The open book: its database and its id (nullptr: no book); its record is read
    /// and its goals with it, as of one book (setVolume() then names a volume of a series)
    /// @param readOnly The book is open read-only (ProjectManager::isReadOnly()): its database
    /// is a copy that goes when it closes, so its goals and badges do not change
    void setBook(core::ProjectDatabase* database, const QString& bookId, bool readOnly = false);

    /// @brief A book is open
    [[nodiscard]] bool hasBook() const { return m_database != nullptr; }

    /// @brief The open book is open read-only: its goals and badges do not change
    [[nodiscard]] bool isBookReadOnly() const { return m_database != nullptr && m_bookReadOnly; }

    /// @brief The database of the open book; nullptr without one
    [[nodiscard]] core::ProjectDatabase* database() const { return m_database; }

    /// @brief The id of the open book; empty without one
    [[nodiscard]] const QString& bookId() const { return m_bookId; }

    /// @brief How the words of the open book are counted now (the coordinator gives it)
    void setBookWordsSource(std::function<int()> source);

    /// @brief The words of the open book now; 0 without one
    [[nodiscard]] int bookWords() const;

    /// @brief The texts of the open book (of the volume) now, by id: their words now and
    ///        whether they are still written (the coordinator gives it); what the daily goal
    ///        of the schedule counts
    void setPlannedTextsSource(std::function<QHash<QString, core::PlannedText>()> source);

    /// @brief The words the open book had when today's writing day started, as the record of
    /// today says: what the deadline counts from
    [[nodiscard]] int bookWordsAtStartOfToday() const;

    /// @brief The goals of the open book; empty without one
    [[nodiscard]] const core::BookGoals& goals() const { return m_goals; }

    /// @brief Keep new goals of the open book in its project.db
    /// @return false without a book or when they cannot be written
    bool setGoals(const core::BookGoals& goals);

    /// @brief The schedule of the open book (of the volume of a series); empty without one
    [[nodiscard]] const core::WritingPlan& plan() const { return m_plan; }

    /// @brief Keep a new schedule of the open book in its project.db; when the daily goal
    ///        comes from it, goalsChanged() too
    /// @return false without a book, with a book open read-only or when it cannot be written
    bool setPlan(const core::WritingPlan& plan);

    /// @brief The volume of a series whose goals and words the book has: its goals in the
    ///        setting @p goalsKey (core::BookGoals::settingKeyOf()), its texts @p elementIds;
    ///        an empty key: one book, its goals in core::BookGoals::SETTING_KEY and all its
    ///        texts. The goals and the schedule are read again when they are of another
    ///        volume.
    void setVolume(const QString& goalsKey, const QSet<QString>& elementIds);

    /// @brief The record of the texts of the volume (setVolume()), with what is not saved yet;
    ///        bookStatistics() of one book
    [[nodiscard]] core::WritingStatistics volumeStatistics() const;

    /// @brief How the words of the whole series are counted now; -1: not a series
    void setSeriesWordsSource(std::function<int()> source);

    /// @brief The words of the whole series now; -1 without one
    [[nodiscard]] int seriesWords() const;

    /// @brief The writer's rules (Settings > Statistics and Goals)
    [[nodiscard]] const core::WritingRules& rules() const { return m_rules; }

public slots:
    /// @brief Take the rules of the settings again (called by name when they change)
    void reloadRules();

public:
    /// @brief The record the daily and weekly goals count, with what is not saved yet: of all
    /// the books or of the open one
    [[nodiscard]] core::WritingStatistics goalStatistics() const;

    /// @brief The record of the open book, with what is not saved yet
    [[nodiscard]] core::WritingStatistics bookStatistics() const;

    /// @brief The record of the journal, with what is not saved yet
    [[nodiscard]] core::WritingStatistics journalStatistics() const;

    /// @brief Now; the tests set their own clock
    [[nodiscard]] QDateTime now() const;

    /// @brief A clock of the tests; empty: the real one
    void setClock(std::function<QDateTime()> clock);

    /// @brief The writing day of now
    [[nodiscard]] QDate today() const;

    /// @brief The pieces of @p minutes of a writing day of @p statistics, from its start (the
    /// end of the day of the rules); a multiple of ACTIVITY_SLOT_MINUTES that divides a day
    static QList<DayPiece> dayPieces(const core::WritingStatistics& statistics, const QDate& day,
                                     int minutes);

    /// @brief The minutes of a writing day that its graph shows, from the start of the day:
    /// the hours @p fromHour to @p toHour of the clock (24 is midnight; the whole day when
    /// they cross its start), wider when @p pieces of @p pieceMinutes have writing outside
    /// them or @p nowMinute is outside; they start and end on whole hours of the clock
    /// @param dayStart The end of a writing day of the rules, in minutes after midnight
    /// @return The first minute and the minute after the last one
    static std::pair<int, int> shownMinutes(const QList<DayPiece>& pieces, int pieceMinutes,
                                            int dayStart, int fromHour, int toHour,
                                            int nowMinute);

    /// @brief The minutes of the clock from the start of writing day @p day to @p when, as
    /// the slots of the record count them; may be outside 0 to 1440
    [[nodiscard]] qint64 minutesIntoDay(const QDate& day, const QDateTime& when) const;

    /// @brief The words of the daily goal today: the daily goal of the rules, or what the
    /// deadline or the schedule of the book needs when its goals say so
    [[nodiscard]] int dailyGoalToday() const;

    /// @brief Where dailyGoalToday() comes from: the deadline while it is ahead and the book
    /// has a length, the schedule while it plans a text, otherwise the writer's own goal
    [[nodiscard]] DailyGoalSource dailyGoalSourceToday() const;

    /// @brief What the schedule of the book needs today (core::dailyGoalForPlan()), whatever
    /// the goals say; 0 without a book
    /// @param texts When given, gets the texts that make it up
    [[nodiscard]] int dailyGoalForPlanToday(QStringList* texts = nullptr) const;

signals:
    /// @brief The numbers changed: something was written, saved or the time went on
    void changed();

    /// @brief The goals of the book changed (setGoals(), another book)
    void goalsChanged();

    /// @brief The schedule of the book changed (setPlan(), another book or volume)
    void planChanged();

private:
    /// @brief changed() soon, at most every CHANGE_INTERVAL_MS
    void schedule();

    /// @brief Read the record of the open book, its goals and its schedule
    void readBook();

    /// @brief Read the goals and the schedule of the open book or of the volume
    void readGoals();

    /// @brief The setting of the goals of the open book or of the volume
    [[nodiscard]] QString goalsKey() const;

    core::WritingRules m_rules;
    core::WritingJournal m_journal;
    core::WritingStatistics m_journalRecord;  ///< Saved in the journal
    core::WritingStatistics m_bookRecord;     ///< Saved in the book
    core::BookGoals m_goals;
    core::WritingPlan m_plan;
    core::ProjectDatabase* m_database{nullptr};
    QString m_bookId;
    bool m_bookReadOnly{false};
    // Plain, cleared when the collector goes: clang-tidy's analyzer reports a QPointer as a use
    // of freed memory
    editor::StatisticsCollector* m_collector{nullptr};
    std::function<int()> m_bookWords;
    std::function<int()> m_seriesWords;
    std::function<QHash<QString, core::PlannedText>()> m_plannedTexts;
    QString m_goalsKey;          ///< The setting of the goals of the volume; empty: one book
    QSet<QString> m_volumeTexts;  ///< The texts of the volume (m_goalsKey not empty)
    std::function<QDateTime()> m_clock;
    QTimer* m_changeTimer{nullptr};  ///< schedule()
    QTimer* m_minuteTimer{nullptr};  ///< The time goes on
    int m_settingsListener{-1};
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WRITING_STATS_MODEL_H
