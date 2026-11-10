/// @file writing_awards.h
/// @brief The badges of writing and the writer's challenges: what each badge counts, how far
/// the writer is, and how far a challenge is
///
/// Fifteen kinds of badges, each with four thresholds, so sixty badges: eight are the
/// writer's own (all the books, for good), four are of the open book (they can start over) and
/// three are of the month (they start over on its first day). Every threshold reached is a
/// badge of its own. The journal keeps the badges won (WritingJournal::badges()), the book the
/// badges of the book (BookBadges, in its project.db).

#pragma once

#include "kalahari/core/writing_statistics.h"

#include <QDate>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <array>
#include <functional>
#include <optional>

namespace kalahari::core {

/// @brief The kinds of badges
enum class BadgeKind {
    // The writer's own: all the books, for good
    Words,          ///< All the words written
    WritingDays,    ///< The days with at least the minimum of a day
    Streak,         ///< The longest run of days in a row
    DailyGoals,     ///< The days with the daily goal reached, as it was then
    WeeklyGoals,    ///< The weeks with the weekly goal reached
    EarlyBird,      ///< The weeks with the most words in the morning (5:00-12:00)
    NightOwl,       ///< The weeks with the most words in the evening and at night (18:00-4:00)
    FinishedBooks,  ///< The books that reached their length
    // The open book's: they can start over
    BookGoal,       ///< The progress of the length of the book, in percent
    ChaptersDone,   ///< The chapters marked final: the first, a quarter, half, all
    Deadlines,      ///< The deadlines kept: the part reached its words before the day
    SteadyPace,     ///< The longest run of weeks in a row with the weekly goal, in the book
    // The month's: they start over on its first day
    MonthGoalDays,  ///< The writing days of the month with the daily goal, in percent; given
                    ///< when the month is over
    MonthWords,     ///< The words of the month against its plan, in percent
    MonthWeeks,     ///< The weeks of the month with the weekly goal: one, two, three, all
};

/// @brief The kinds of badges, in the order they are shown
constexpr std::array<BadgeKind, 15> BADGE_KINDS{
    BadgeKind::Words,         BadgeKind::WritingDays, BadgeKind::Streak,
    BadgeKind::DailyGoals,    BadgeKind::WeeklyGoals, BadgeKind::EarlyBird,
    BadgeKind::NightOwl,      BadgeKind::FinishedBooks, BadgeKind::BookGoal,
    BadgeKind::ChaptersDone,  BadgeKind::Deadlines,   BadgeKind::SteadyPace,
    BadgeKind::MonthGoalDays, BadgeKind::MonthWords,  BadgeKind::MonthWeeks};

/// @brief The thresholds of each kind: bronze, silver, gold and platinum
constexpr int BADGE_LEVELS = 4;

/// @brief Whose a badge is
enum class BadgeGroup { Yours, Book, Month };

/// @brief Whose badges of @p kind are
BadgeGroup badgeGroupOf(BadgeKind kind);

/// @brief The name of @p kind in the records ("words", "streak"...)
QString badgeIdOf(BadgeKind kind);

/// @brief The kind named @p id in the records; none for a name not known
std::optional<BadgeKind> badgeKindOf(const QString& id);

/// @brief How far the writer is with a kind of badges
struct BadgeProgress {
    BadgeKind kind = BadgeKind::Words;
    int value = 0;                       ///< What it counts now: words, days, weeks, percent
    std::array<int, BADGE_LEVELS> thresholds{};  ///< The value of each badge, rising
    int done = 0;                        ///< Of a share: the days, chapters or weeks done
    int total = 0;                       ///< Of a share: of how many; 0: not a share
    bool available = true;               ///< It can be won now (a book with a length...)

    /// @brief The badges of the kind that the value reaches, 0 to 4
    int level() const;

    /// @brief How far the value is to the badge @p level (0 to 3), 0 to 1
    double share(int level) const;
};

/// @brief What the badges are counted from
struct AwardsInput {
    QDate today;                     ///< The writing day of now
    WritingStatistics journal;       ///< The record of all the books
    WritingStatistics goals;         ///< The record the daily and weekly goals count
    QMap<QDate, int> dailyGoals;     ///< The daily goal each day had; other days: the rules
    QMap<QDate, int> weeklyGoals;    ///< The weekly goal each day had; other days: the rules
    int finishedBooks = 0;           ///< The books that reached their length

    bool hasBook = false;            ///< A book is open
    WritingStatistics book;          ///< The record of the open book
    QDate bookSince;                 ///< Its badges count from this day; invalid: from its start
    int bookWords = 0;               ///< The words of the book now
    int bookTarget = 0;              ///< Its length; 0: none
    int chaptersDone = 0;            ///< Its chapters marked final
    int chapters = 0;                ///< All its chapters
    int deadlinesKept = 0;           ///< Its deadlines kept

    /// @brief The daily goal of @p day: as it was then, or the rules'
    int dailyGoalOf(const QDate& day) const;

    /// @brief The weekly goal of the week of @p monday: as it was on its last day kept, or
    /// the rules'
    int weeklyGoalOf(const QDate& monday) const;
};

/// @brief How far the writer is with each kind, in the order of BADGE_KINDS
QList<BadgeProgress> badgeProgress(const AwardsInput& input);

/// @brief How far the badges of the month of @p month are as of @p asOf: the month is over
/// when @p asOf is after it (MonthGoalDays, MonthWords and MonthWeeks)
QList<BadgeProgress> monthBadgeProgress(const AwardsInput& input, const QDate& month,
                                        const QDate& asOf);

/// @brief The longest run of days in a row in @p statistics from @p from to @p today, by its
/// rules (WritingStatistics::streak())
int longestStreak(const WritingStatistics& statistics, const QDate& from, const QDate& today);

/// @brief The Mondays of the weeks of the month of @p month: the weeks whose Thursday is in it
QList<QDate> weeksOfMonth(const QDate& month);

/// @brief The scope of the badges of the month of @p day in the records: "2026-11"
QString monthScope(const QDate& day);

/// @brief A badge won
struct EarnedBadge {
    BadgeKind kind = BadgeKind::Words;
    int level = 0;   ///< 0 bronze ... 3 platinum
    QDate date;      ///< The day it was won
    QString scope;   ///< Of the writer: empty; of a month: monthScope(); of a book: its id

    bool operator==(const EarnedBadge&) const = default;
};

/// @brief The badges of a book, kept in its project.db (SETTING_KEY)
struct BookBadges {
    static constexpr const char* SETTING_KEY = "writing.badges";

    QDate since;                  ///< They count from this day (Start Over); invalid: always
    QList<EarnedBadge> earned;    ///< Won since then
    QList<EarnedBadge> history;   ///< Won before Start Over
    QStringList keptDeadlines;    ///< The deadlines kept (deadlineKey())

    QString toJson() const;
    static BookBadges fromJson(const QString& text);

    bool operator==(const BookBadges&) const = default;
};

/// @brief The kinds of challenges
enum class ChallengeKind {
    Words,        ///< Write so many words
    DaysInRow,    ///< Write so many days in a row
    WritingDays,  ///< Write on so many days (at least the minimum of a day)
};

/// @brief A challenge the writer set
struct WritingChallenge {
    int id = 0;            ///< Its number in the journal
    ChallengeKind kind = ChallengeKind::Words;
    int target = 0;        ///< The words or the days
    QDate start;           ///< The first writing day it counts
    QDate end;             ///< The last one
    QDate done;            ///< The day it was done; invalid: not yet

    bool operator==(const WritingChallenge&) const = default;
};

/// @brief The name of @p kind in the records
QString challengeIdOf(ChallengeKind kind);

/// @brief The kind named @p id; Words for a name not known
ChallengeKind challengeKindOf(const QString& id);

/// @brief How far a challenge is on @p today in @p goals (the record the goals count)
struct ChallengeProgress {
    int value = 0;      ///< The words or days so far
    bool done = false;  ///< The target is reached (now or before)
    bool over = false;  ///< Its last day has passed without it done
    int daysLeft = 0;   ///< The days left, today with them; 0 when over or done
};
ChallengeProgress challengeProgress(const WritingChallenge& challenge,
                                    const WritingStatistics& goals, const QDate& today);

}  // namespace kalahari::core
