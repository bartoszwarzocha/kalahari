// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_awards_tracker.h
/// @brief The badges and challenges of the writer as the writing goes on: which are won, which
/// goals are reached and which deadlines are coming
///
/// It follows the model of the statistics: a little after the numbers change it counts the
/// badges again (core::badgeProgress()), keeps the new ones (the writer's and the month's in the
/// journal, the book's in the book) and says what was won or reached, once. The messages are
/// shown by WritingStatsCoordinator, as the settings say.

#pragma once

#include <kalahari/core/writing_awards.h>
#include <kalahari/core/writing_goals.h>

#include <QDate>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

class QTimer;

namespace kalahari::gui {

class WritingStatsModel;

/// @brief The badges and challenges of the writer as the writing goes on
class WritingAwardsTracker : public QObject {
    Q_OBJECT

public:
    /// @brief Counts again this long after the numbers change
    static constexpr int INTERVAL_MS = 1500;

    /// @brief The open book, as the badges of the book need it
    struct BookSource {
        std::function<std::pair<int, int>()> chapters;      ///< Its chapters marked final, and all
        std::function<int(const QString& elementId)> words;  ///< The words of a part now
    };

    /// @param model The statistics; it outlives the tracker
    explicit WritingAwardsTracker(WritingStatsModel* model, QObject* parent = nullptr);

    /// @brief Where the chapters of the book and the words of its parts come from
    void setBookSource(BookSource source);

    /// @brief Count the badges again now, keep the new ones and say what changed
    void evaluate();

    /// @brief Count them now when they were not counted yet, so that a view has them
    void countIfNeeded();

    /// @brief They were counted at least once
    bool counted() const { return m_counted; }

    /// @brief How far each kind of badges is, as last counted (BADGE_KINDS order)
    const QList<core::BadgeProgress>& progress() const { return m_progress; }

    /// @brief How far @p kind is
    core::BadgeProgress progressOf(core::BadgeKind kind) const;

    /// @brief The badges won that the tab shows: the writer's, of this month and of the open
    /// book since it started over
    QList<core::EarnedBadge> shownBadges() const;

    /// @brief The badge @p level of @p kind is won (in the scope the tab shows)
    std::optional<core::EarnedBadge> earned(core::BadgeKind kind, int level) const;

    /// @brief The badge won last, of all the badges shown; none when none
    std::optional<core::EarnedBadge> lastBadge() const;

    /// @brief The months in which the badge @p level of a kind of the month was won
    int monthsWon(core::BadgeKind kind, int level) const;

    /// @brief The badges of the open book (empty without one)
    const core::BookBadges& bookBadges() const { return m_bookBadges; }

    /// @brief The badges of the book count again from today; those won stay in its history
    void startBookOver();

    /// @brief The writer's challenges
    const QList<core::WritingChallenge>& challenges() const { return m_challenges; }

    /// @brief How far @p challenge is today
    core::ChallengeProgress challengeProgress(const core::WritingChallenge& challenge) const;

    /// @brief Set a new challenge; it turns the challenges on (the setting stats.challenges)
    /// @return Its number; 0 when it cannot be kept
    int addChallenge(const core::WritingChallenge& challenge);

    /// @brief Forget the challenge @p id
    bool removeChallenge(int id);

    /// @brief The key of a deadline among those kept (BookBadges::keptDeadlines)
    static QString deadlineKey(const core::WritingDeadline& deadline);

signals:
    /// @brief The badges, the challenges or their progress changed
    void changed();

    /// @brief New badges were won (only with the setting stats.badges on)
    void badgesWon(const QList<core::EarnedBadge>& badges);

    /// @brief A challenge was done
    void challengeDone(const core::WritingChallenge& challenge, int value);

    /// @brief The daily goal was reached just now
    void dailyGoalReached(int words, int goal);

    /// @brief The weekly goal was reached just now
    void weeklyGoalReached(int words, int goal);

    /// @brief A deadline of the book is coming: in @p days days (0: today)
    void deadlineComing(const QString& name, const QDate& date, int days, int words, int target);

private:
    /// @brief Count again soon
    void schedule();

    /// @brief Read what is kept: the badges, the challenges, the badges of the book
    void readKept();

    /// @brief What the badges are counted from now
    core::AwardsInput input() const;

    /// @brief Keep the goals of today in the journal when they changed
    void keepDayGoals(const core::AwardsInput& input);

    /// @brief The book finished and the deadlines kept, when they are
    void keepBookFacts();

    /// @brief The daily and weekly goals reached and the deadlines coming, once
    void checkGoals();

    WritingStatsModel* m_model = nullptr;
    BookSource m_source;
    QTimer* m_timer = nullptr;
    QList<core::BadgeProgress> m_progress;
    QList<core::EarnedBadge> m_journalBadges;  ///< The writer's and the months'
    QList<core::WritingChallenge> m_challenges;
    QStringList m_finishedBooks;
    core::BookBadges m_bookBadges;
    QString m_bookId;                          ///< The book m_bookBadges are of
    QDate m_keptGoalsDay;                      ///< The day whose goals were kept last
    std::pair<int, int> m_keptGoals{-1, -1};
    bool m_counted = false;                    ///< Counted at least once, with what is kept
    QDate m_goalsDay;                          ///< The day the goals were checked
    bool m_dailyReached = false;
    bool m_weeklyReached = false;
    QSet<QString> m_deadlinesSaid;             ///< The deadlines said today
};

}  // namespace kalahari::gui
